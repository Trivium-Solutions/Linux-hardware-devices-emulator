
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/ctype.h>
#include <linux/version.h>

#include "vcpsim.h"

struct iface_struct {
	struct kobject kobj;
	struct list_head dev_list;
};

#define to_iface(p) container_of(p, struct iface_struct, kobj)

struct iface_attribute {
	struct attribute attr;
	ssize_t (*show)(struct iface_struct * iface, struct iface_attribute * attr, char * buf);
	ssize_t (*store)(struct iface_struct * iface, struct iface_attribute * attr, const char * buf, size_t count);
};

#define to_iface_attr(p) container_of(p, struct iface_attribute, attr)

struct dev_struct {
	struct kobject kobj;
	struct iface_struct * iface;
	struct list_head list;
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

static ssize_t iface_attr_show(struct kobject * kobj, struct attribute * attr, char *buf)
{
	struct iface_struct * ifc = to_iface(kobj);
	struct iface_attribute * a = to_iface_attr(attr);

	if (a->show)
		return a->show(ifc, a, buf);

	return -EIO;
}

static ssize_t iface_attr_store(struct kobject * kobj, struct attribute * attr, const char * buf, size_t len)
{
	struct iface_struct * ifc = to_iface(kobj);
	struct iface_attribute * a = to_iface_attr(attr);

	if (a->store)
		return a->store(ifc, a, buf, len);

	return -EIO;
}

static const struct sysfs_ops iface_sysfs_ops = {
	.show = iface_attr_show,
	.store = iface_attr_store,
};

/*
// Stubs for the interface attributes.

static ssize_t iface_show(struct iface_struct * iface, struct iface_attribute * attr,
			char * buf)
{
	return sprintf(buf, "%s/%s\n", iface->kobj.name, attr->attr.name);
}

static ssize_t iface_store(struct iface_struct * iface, struct iface_attribute * attr,
			 const char * buf, size_t count)
{
	return count;
}
*/

/* forward declaration */
static struct kobj_type dev_ktype;

static struct dev_struct * new_dev(enum VS_IFACE iface) {
	struct iface_struct * ifc = &ifaces[iface];
	char name[16];
	int err;
	struct dev_struct * ret;

	ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (!ret)
		return NULL;

	new_device_name(iface, name, sizeof(name));

	err = kobject_init_and_add(&ret->kobj, &dev_ktype, &ifc->kobj, name);

	if (err) {
		pr_err("kobject_init_and_add() failed\n");
		kobject_put(&ret->kobj);
		/* don't do kfree(ret) as dev_release will take care of that */
		return NULL;
	}

	// TODO device-specific initialization

	list_add(&ret->list, &ifc->dev_list);

	kobject_uevent(&ret->kobj, KOBJ_ADD);

	return ret;
}


static ssize_t iface_add_store(struct iface_struct * iface, struct iface_attribute * attr, const char * buf, size_t count)
{
	int ret = -EIO;
	enum VS_IFACE ifc;
	struct dev_struct * dev;
	const char * iface_name = kobject_name(&iface->kobj);

	if (count == 0)
		pr_debug("empty write data\n");
	else
	if (!str_to_iface(iface_name, &ifc))
		pr_err("unsupported interface: %s\n", iface_name);
	else
	if (!(dev = new_dev(ifc)))
		pr_err("couldn't create new device: %s\n", iface_name);
	else {
		ret = count;
		pr_debug("%s: new device created\n", kobject_name(&dev->kobj));
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

static struct dev_struct * find_device(struct iface_struct * iface, const char * name)
{
	struct dev_struct * dev;

	list_for_each_entry (dev, &iface->dev_list, list)
		if (strcmp(kobject_name(&dev->kobj), name) == 0)
			return dev;

	return NULL;
}

static ssize_t iface_uninstall_store(struct iface_struct * iface, struct iface_attribute * attr, const char * buf, size_t count)
{
	int ret = -EIO;
	const char * iface_name = kobject_name(&iface->kobj);
	enum VS_IFACE ifc;
	char dev_name[16];
	struct dev_struct * dev;

	if (count == 0)
		pr_debug("empty write data\n");
	else
	if (!str_to_iface(iface_name, &ifc))
		pr_err("unsupported interface: %s\n", iface_name);
	else
	if (!copy_word(buf, count, dev_name, sizeof(dev_name)))
		pr_err("malformed device identifier written to %s/%s\n", iface_name, attr->attr.name);
	else
	if (!(dev = find_device(&ifaces[ifc], dev_name)))
		pr_err("%s: device not found\n", dev_name);
	else {
		pr_debug("%s: uninstalling device\n", kobject_name(&dev->kobj));

		kobject_put(&dev->kobj);

		ret = count;
	}

	return ret;
}

#define PERMS_RO 0444
#define PERMS_WO 0200
#define PERMS_RW 0664

#define DEF_ATTR_RO(__pfx, __name) \
static struct __pfx##_attribute __name##_attr = \
	__ATTR(__name, PERMS_RO, __pfx##_##__name##_show, NULL)

#define DEF_ATTR_WO(__pfx, __name) \
static struct __pfx##_attribute __name##_attr = \
	__ATTR(__name, PERMS_WO, NULL, __pfx##_##__name##_store)

#define DEF_ATTR_RW(__pfx, __name) \
static struct __pfx##_attribute __name##_attr = \
	__ATTR(__name, PERMS_RW, __pfx##_##__name##_show, __pfx##_##__name##_store)


/* All attributes (files in a sysfs directory) for the interface.
 * If you want to add/remove an interface, you should start here. */
#define FOREACH_IFACE_ATTR(A)\
	A(add, WO)		\
	A(uninstall, WO)	\


#define DEF_ATTR(__name, __perm)	DEF_ATTR_##__perm(iface, __name);
FOREACH_IFACE_ATTR(DEF_ATTR)
#undef DEF_ATTR

#define DEF_ATTR(__name, __perm)	&__name##_attr.attr,
static struct attribute * iface_default_attrs[] = {
	FOREACH_IFACE_ATTR(DEF_ATTR)
	NULL
};
#undef DEF_ATTR

ATTRIBUTE_GROUPS(iface_default);

static struct kobj_type iface_ktype = {
	.sysfs_ops = &iface_sysfs_ops,
	.default_groups = iface_default_groups,
};


static ssize_t dev_attr_show(struct kobject * kobj, struct attribute * attr, char *buf)
{
	struct dev_struct * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->show)
		return a->show(d, a, buf);

	return -EIO;
}

static ssize_t dev_attr_store(struct kobject * kobj, struct attribute * attr, const char * buf, size_t len)
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

	// TODO device-specific deinitialization

	list_del(&dev->list);

	kfree(dev);

	pr_debug("%s: device released\n", kobject_name(kobj));
}

static ssize_t dev_show(struct dev_struct * dev, struct dev_attribute * attr, char * buf)
{
	return sprintf(buf, "%s/%s\n", kobject_name(&dev->kobj), attr->attr.name);
}

static ssize_t dev_store(struct dev_struct * dev, struct dev_attribute * attr, const char * buf, size_t count)
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

#define DEF_ATTR(__name, __perm)	DEF_ATTR_##__perm(dev, __name);
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


static int init_iface(enum VS_IFACE iface)
{
	struct iface_struct * ifc = &ifaces[iface];
	int err;

	INIT_LIST_HEAD(&ifc->dev_list);

	err = kobject_init_and_add(&ifc->kobj, &iface_ktype, &base_kset->kobj, iface_to_str(iface));

	return err;
}

static void cleanup_iface(enum VS_IFACE iface)
{
	struct iface_struct * ifc = &ifaces[iface];
	struct list_head * dl = &ifc->dev_list;

	while (!list_empty(dl)) {
		struct dev_struct * dev = list_first_entry(dl, struct dev_struct, list);

		kobject_put(&dev->kobj);
	}

	kobject_put(&ifc->kobj);
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
			pr_err("init_iface() failed\n");

			/* clean up the interfaces installed so far */
			for (i--; i >= 0; i--)
				cleanup_iface((enum VS_IFACE)i);

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

