
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/ctype.h>
#include <linux/version.h>

#include "vcpsim.h"

struct iface_struct {
	struct kset * kset;
	struct kobj_attribute add;
	struct kobj_attribute uninstall;
};

struct dev_struct {
	struct kobject kobj;
};

#define to_dev(p) container_of(p, struct dev_struct, kobj)

struct dev_attribute {
	struct attribute attr;
	ssize_t (*show)(struct dev_struct * dev, struct dev_attribute * attr, char * buf);
	ssize_t (*store)(struct dev_struct * dev, struct dev_attribute * attr, const char * buf, size_t count);
};

#define to_dev_attr(p) container_of(p, struct dev_attribute, attr)

static struct kset * base_kset;
static struct iface_struct ifaces[VS_IFACE_COUNT];

static ssize_t dev_attr_show(struct kobject * kobj,
			     struct attribute * attr,
			     char *buf)
{
	struct dev_struct * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->show)
		return a->show(d, a, buf);

	return -EIO;
}

static ssize_t dev_attr_store(struct kobject * kobj,
			      struct attribute * attr,
			      const char * buf, size_t len)
{
	struct dev_struct * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->store)
		return a->store(d, a, buf, len);

	return -EIO;
}

static const struct sysfs_ops dev_sysfs_ops = {
	.show = dev_attr_show,
	.store = dev_attr_store,
};

static void dev_release(struct kobject *kobj)
{
	struct dev_struct * dev = to_dev(kobj);

	// TODO free associated resources

	kfree(dev);

	pr_debug("%s: device released\n", kobj->name);
}

static ssize_t dev_show(struct dev_struct * dev, struct dev_attribute * attr,
			char * buf)
{
	return sprintf(buf, "%s.%s\n", dev->kobj.name, attr->attr.name);
}

static ssize_t dev_store(struct dev_struct * dev, struct dev_attribute * attr,
			 const char * buf, size_t count)
{

	return count;
}

/* XXX temporary stubs */
#define	dev_count_show		dev_show
#define	dev_pairs_show		dev_show
#define	dev_pairs_store		dev_store
#define	dev_delete_store	dev_store
#define	dev_clear_store		dev_store

/* All attributes (files in a sysfs directory) for the device.
 * If you want to add/remove an attribute, you should start here. */
#define FOREACH_DEV_ATTR(A)\
	A(count, RO)	\
	A(pairs, RW)	\
	A(delete, WO)	\
	A(clear, WO)	\

#define PERMS_RO 0444
#define PERMS_WO 0200
#define PERMS_RW 0664

#define DEF_ATTR_RO(__name) \
static struct dev_attribute __name##_attr = \
	__ATTR(__name, PERMS_RO, dev_##__name##_show, NULL)

#define DEF_ATTR_WO(__name) \
static struct dev_attribute __name##_attr = \
	__ATTR(__name, PERMS_WO, NULL, dev_##__name##_store)

#define DEF_ATTR_RW(__name) \
static struct dev_attribute __name##_attr = \
	__ATTR(__name, PERMS_RW, dev_##__name##_show, dev_##__name##_store)

#define DEF_ATTR(__name, __perm)	DEF_ATTR_##__perm(__name);
FOREACH_DEV_ATTR(DEF_ATTR)
#undef DEF_ATTR

#define DEF_ATTR(__name, __perm)	&__name##_attr.attr,
static struct attribute * dev_default_attrs[] = {
	FOREACH_DEV_ATTR(DEF_ATTR)
	NULL
};
#undef DEF_ATTR

ATTRIBUTE_GROUPS(dev_default);

static struct kobj_type dev_ktype = {
	.sysfs_ops = &dev_sysfs_ops,
	.release = dev_release,
	.default_groups = dev_default_groups,
};


static struct dev_struct * new_dev(enum VS_IFACE iface) {
	struct iface_struct * ifc = &ifaces[iface];
	char name[16];
	int err;
	struct dev_struct * ret;

	ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (!ret)
		return NULL;

	new_device_name(iface, name, sizeof(name));

	ret->kobj.kset = ifc->kset;

	err = kobject_init_and_add(&ret->kobj, &dev_ktype, NULL, name);

	if (err) {
		pr_err("kobject_init_and_add() failed\n");
		kobject_put(&ret->kobj);
		/* don't do kfree(ret) as dev_release will take care of that */
		return NULL;
	}

	// TODO further initialization

	kobject_uevent(&ret->kobj, KOBJ_ADD);

	return ret;
}


static ssize_t sysfs_add_dev(struct kobject * kobj,
			      struct kobj_attribute * attr,
			      const char * buf, size_t count)
{
	int ret = -EIO;
	enum VS_IFACE iface;
	struct dev_struct * dev;

	if (count == 0) 
		pr_debug("empty write data\n");
	else
	if (!str_to_iface(kobj->name, &iface))
		pr_err("unsupported interface: %s\n", kobj->name);
	else
	if (!(dev = new_dev(iface)))
		pr_err("couldn't create new device: %s\n", kobj->name);
	else {
		ret = count;
		pr_debug("%s: new device created\n", dev->kobj.name);
	}

	return ret;
}

static inline int copy_word(const char * src, size_t src_len, char * dst, size_t dst_len)
{
	char * s;

	/* skip leading spaces */
	for (; src_len && isspace((unsigned char)*src); src_len--, src++)
		;

	if (!src_len || dst_len < 1)
		return 0;

	/* copy non-blank printable characters, observing the boundaries */
	for (s = dst;
	     src_len && dst_len > 1 && *src && !isspace((unsigned char)*src) && isprint((unsigned char)*src);
	     src_len--, dst_len--)
		*s++ = *src++;

	/* ensure the terminating null */
	*s = 0;

	return s - dst;
}

static ssize_t sysfs_uninstall_dev(struct kobject * kobj,
			      struct kobj_attribute * attr,
			      const char * buf, size_t count)
{
	int ret = -EIO;
	enum VS_IFACE iface;
	char dev_name[16];
	struct kobject * ko;

	if (count == 0)
		pr_debug("empty write data\n");
	else
	if (!str_to_iface(kobj->name, &iface)) 
		pr_err("unsupported interface: %s\n", kobj->name);
	else
	if (!copy_word(buf, count, dev_name, sizeof(dev_name)))
		pr_err("malformed device identifier written to %s/%s\n", kobj->name, attr->attr.name);
	else
	if (!(ko = kset_find_obj(ifaces[iface].kset, dev_name)))
		pr_err("%s: device not found\n", dev_name);
	else {
		pr_debug("%s: uninstalling device\n", ko->name);

		/* XXX release the reference taken by kset_find_obj(). */
		kobject_put(ko);

		/* do the job */
		kobject_put(ko);

		ret = count;
	}

	return ret;
}


static int init_iface(enum VS_IFACE iface)
{
	struct iface_struct * ifc = &ifaces[iface];
	int err;
	struct kset * kset;

	/* create the interface directory */
	kset = kset_create_and_add(iface_to_str(iface), NULL, &base_kset->kobj);

	if (!kset)
		return -ENOMEM;

	ifc->kset = kset;

	/* create the add file */
	ifc->add.attr.name = "add";
	ifc->add.attr.mode = 0200;
	ifc->add.store = sysfs_add_dev;

	err = sysfs_create_file(&kset->kobj, &ifc->add.attr);

	if (err) {
		kset_unregister(ifc->kset);
		return err;
	}

	/* create the uninstall file */
	ifc->uninstall.attr.name = "uninstall";
	ifc->uninstall.attr.mode = 0200;
	ifc->uninstall.store = sysfs_uninstall_dev;

	err = sysfs_create_file(&kset->kobj, &ifc->uninstall.attr);

	if (err) {
		sysfs_remove_file(&kset->kobj, &ifc->add.attr);
		kset_unregister(ifc->kset);
		return err;
	}

	return 0;
}

static void cleanup_iface(enum VS_IFACE iface)
{
	struct iface_struct * ifc = &ifaces[iface];

	sysfs_remove_file(&ifc->kset->kobj, &ifc->uninstall.attr);
	sysfs_remove_file(&ifc->kset->kobj, &ifc->add.attr);
	kset_unregister(ifc->kset);
}

int vs_init_sysfs(void)
{
	int i;
	int err;

	pr_debug("creating sysfs entries\n");

	base_kset = kset_create_and_add(DRIVER_NAME, NULL, kernel_kobj);

	if (!base_kset)
		return -ENOMEM;

	for (i = 0; i < VS_IFACE_COUNT; i++) {
		err = init_iface((enum VS_IFACE)i);
		if (err) {
			// TODO clean-up
			pr_err("init_iface() failed\n");
			return err;
		}
	}

	pr_info("sysfs entries created\n");
	return 0;
}

void vs_cleanup_sysfs(void)
{
	int i;

	pr_debug("cleaning up sysfs entries\n");

	for (i = VS_IFACE_COUNT - 1; i >= 0; i--)
		cleanup_iface((enum VS_IFACE)i);

	kset_unregister(base_kset);

	pr_info("sysfs entries cleaned up\n");
}

