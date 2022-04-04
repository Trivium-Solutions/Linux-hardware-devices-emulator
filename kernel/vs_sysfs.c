
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/bitmap.h>

#include "vcpsim.h"

/* Comment this in, if you want to have individual pair operations
 * (add/delete/etc) logged in debug mode. */
//#define LOG_PAIRS 1

struct vs_iface {
	struct kobject kobj;
	struct list_head dev_list;
};

#define to_iface(p) container_of(p, struct vs_iface, kobj)

struct iface_attribute {
	struct attribute attr;
	ssize_t (*show)(struct vs_iface * iface,
		struct iface_attribute * attr, char * buf);
	ssize_t (*store)(struct vs_iface * iface,
		struct iface_attribute * attr, const char * buf, size_t count);
};

#define to_iface_attr(p) container_of(p, struct iface_attribute, attr)

struct vs_dev {
	struct kobject kobj;
	struct list_head entry;
	struct list_head pair_list;
	enum VS_IFACE iface;
	long index;
	struct vs_dev_priv * device;
	struct kobject * pairs_kobj;
	DECLARE_BITMAP(pairs_indexes, VS_MAX_PAIRS);
};

#define to_dev(p) container_of(p, struct vs_dev, kobj)

struct dev_attribute {
	struct attribute attr;
	ssize_t (*show)(struct vs_dev * dev,
		struct dev_attribute * attr, char * buf);
	ssize_t (*store)(struct vs_dev * dev,
		struct dev_attribute * attr, const char * buf, size_t count);
};

#define to_dev_attr(p) container_of(p, struct dev_attribute, attr)

struct vs_dev_ops {
	struct vs_dev_priv * (*create)(struct vs_dev * dev, long index);
	void (*destroy)(struct vs_dev_priv * device);
};

/* Prototypes for our internal device operations. */
#define DECL_DEVOP(__upper, __lower) \
	extern struct vs_dev_priv * vs_create_##__lower##_device(struct vs_dev * dev, long index); \
	extern void vs_destroy_##__lower##_device(struct vs_dev_priv * device); \

VS_FOREACH_IFACE(DECL_DEVOP)

#undef DECL_DEVOP

/* Internal device operations. */
#define DEVOP(__upper, __lower) { \
	.create = vs_create_##__lower##_device, \
	.destroy = vs_destroy_##__lower##_device, \
},

static const struct vs_dev_ops dev_ops[] = {
	VS_FOREACH_IFACE(DEVOP)
};

#undef DEVOP

static struct kset * base_kset;
static struct vs_iface ifaces[VS_IFACE_COUNT];

static ssize_t iface_attr_show(struct kobject * kobj, struct attribute * attr, char *buf)
{
	struct vs_iface * ifc = to_iface(kobj);
	struct iface_attribute * a = to_iface_attr(attr);

	if (a->show)
		return a->show(ifc, a, buf);

	return -EIO;
}

static ssize_t iface_attr_store(struct kobject * kobj,
	struct attribute * attr, const char * buf, size_t len)
{
	struct vs_iface * ifc = to_iface(kobj);
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

static ssize_t iface_show(struct vs_iface * iface,
	struct iface_attribute * attr, char * buf)
{
	return sprintf(buf, "%s/%s\n", kobject_name(&iface->kobj), attr->attr.name);
}

static ssize_t iface_store(struct vs_iface * iface,
	struct iface_attribute * attr, const char * buf, size_t count)
{
	return count;
}
*/

/* Pools of indexes for devices. */
static DECLARE_BITMAP(indexes[VS_IFACE_COUNT], VS_MAX_DEVICES);

static inline long find_free_dev_index(enum VS_IFACE iface)
{
	long ret = find_first_zero_bit(indexes[iface], VS_MAX_DEVICES);

	if (ret == VS_MAX_DEVICES)
		ret = -1;

	return ret;
}

static inline void take_dev_index(enum VS_IFACE iface, long index)
{
	if (index >= 0)
		set_bit(index, indexes[iface]);
}

static inline void put_dev_index(enum VS_IFACE iface, long index)
{
	if (index >= 0)
		clear_bit(index, indexes[iface]);
}

static struct vs_dev * add_dev(enum VS_IFACE iface, long index)
{
	struct vs_dev * ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (ret) {
		ret->iface = iface;
		ret->index = index;

		take_dev_index(iface, index);

		INIT_LIST_HEAD(&ret->pair_list);
		list_add(&ret->entry, &ifaces[iface].dev_list);
	}

	return ret;
}

static void del_dev(struct vs_dev * dev)
{
	list_del(&dev->entry);

	put_dev_index(dev->iface, dev->index);

	kfree(dev);
}

static void clear_pairs(struct vs_dev * dev);

static void shutdown_dev(struct vs_dev * dev)
{
	/* assume that we may be shutting down
	 * a partially initialized device */
	if (dev->device)
		dev_ops[dev->iface].destroy(dev->device);

	clear_pairs(dev);

	list_del(&dev->entry);
	put_dev_index(dev->iface, dev->index);

	/* kobject_put() allows its argument to be NULL */
	kobject_put(dev->pairs_kobj);
	kobject_put(&dev->kobj);
	/* kfree(dev) will be done in dev_release() */
}

/* forward declaration */
static struct kobj_type dev_ktype;

static struct vs_dev * new_dev(enum VS_IFACE iface) {
	struct vs_iface * ifc = &ifaces[iface];
	long idx = find_free_dev_index(iface);
	struct vs_dev * ret = NULL;
	int err;

	/* format of directory name: <interface name> <index>, eg tty0 */

	if (idx < 0)
		pr_err("%s: device not created; too many devices",
			iface_to_str(iface));
	else
	if (!(ret = add_dev(iface, idx)))
		pr_err("%s%ld: device not created; out of memory!",
			iface_to_str(iface), idx);
	else
	if (!(ret->device = dev_ops[iface].create(ret, idx))) {
		/* assume a log message was printed by create() */
		del_dev(ret);
		ret = NULL;
	}
	else
	if (!!(err = kobject_init_and_add(&ret->kobj, &dev_ktype, &ifc->kobj,
			"%s%ld", iface_to_str(iface), idx))) {
		pr_err("kobject_init_and_add() failed\n");
		shutdown_dev(ret);
		ret = NULL;
	}
	else
	if (!(ret->pairs_kobj = kobject_create_and_add("pairs", &ret->kobj))) {
		pr_err("kobject_create_and_add() failed\n");
		shutdown_dev(ret);
		ret = NULL;
	}
	else {

		kobject_uevent(&ret->kobj, KOBJ_ADD);
	}

	return ret;
}

static ssize_t iface_add_store(struct vs_iface * iface,
	struct iface_attribute * attr, const char * buf, size_t count)
{
	int ret = -EIO;
	const char * iface_name = kobject_name(&iface->kobj);
	const char * filename = attr->attr.name;
	enum VS_IFACE ifc;
	struct vs_dev * dev;

	if (count == 0)
		pr_err("%s/%s: empty write data\n",
			iface_name, filename);
	else
	if (!str_to_iface(iface_name, &ifc))
		pr_err("%s/%s: unsupported interface: %s\n",
			iface_name, filename, iface_name);
	else
	if (!(dev = new_dev(ifc)))
		pr_err("%s/%s: couldn't create new device with interface %s\n",
			iface_name, filename, iface_name);
	else {
		ret = count;

		pr_debug("%s/%s: %s: new device created\n",
			iface_name, filename, kobject_name(&dev->kobj));
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
	     src_len && dst_len > 1 &&
	     *src && !isspace((unsigned char)*src) &&
	     isprint((unsigned char)*src);
	     src_len--, dst_len--)
		*s++ = *src++;

	/* ensure the terminating null */
	*s = 0;

	return s - dst;
}

static struct vs_iface * iface_from_devname(const char * name)
{
#define CHECK(__upper, __lower) \
	if (strncmp(name, #__upper, sizeof(#__upper) - 1) == 0 || \
	    strncmp(name, #__lower, sizeof(#__lower) - 1) == 0)   \
		return &ifaces[VS_##__upper]; \
	else

	VS_FOREACH_IFACE(CHECK)
		return NULL;
#undef CHECK
}

static struct vs_dev * find_device(struct vs_iface * iface, const char * name)
{
	struct vs_dev * dev;

	list_for_each_entry (dev, &iface->dev_list, entry)
		if (strcmp(kobject_name(&dev->kobj), name) == 0)
			return dev;

	return NULL;
}

static ssize_t iface_uninstall_store(struct vs_iface * iface,
	struct iface_attribute * attr, const char * buf, size_t count)
{
	int ret = -EINVAL;
	const char * iface_name = kobject_name(&iface->kobj);
	const char * filename = attr->attr.name;
	enum VS_IFACE ifc;
	char dev_name[16];
	struct vs_dev * dev;

	if (count == 0)
		pr_err("%s/%s: empty write data\n",
			iface_name, filename);
	else
	if (!str_to_iface(iface_name, &ifc))
		pr_err("%s/%s: %s: unsupported interface\n",
			iface_name, filename, iface_name);
	else
	if (!copy_word(buf, count, dev_name, sizeof(dev_name)))
		pr_err("%s/%s: malformed device identifier\n",
			iface_name, filename);
	else
	if (!(dev = find_device(&ifaces[ifc], dev_name)))
		pr_err("%s/%s: %s: device not found\n",
			iface_name, filename, dev_name);
	else {
		pr_debug("%s/%s: %s: uninstalling device\n",
			iface_name, filename, kobject_name(&dev->kobj));

		shutdown_dev(dev);

		ret = count;
	}

	return ret;
}

#define PERMS_RO 0444
#define PERMS_WO 0200
#define PERMS_RW 0664

#define DEF_ATTR_RO(__pfx, __name) \
static struct __pfx##_attribute __pfx##_##__name##_attr = \
	__ATTR(__name, PERMS_RO, __pfx##_##__name##_show, NULL)

#define DEF_ATTR_WO(__pfx, __name) \
static struct __pfx##_attribute __pfx##_##__name##_attr = \
	__ATTR(__name, PERMS_WO, NULL, __pfx##_##__name##_store)

#define DEF_ATTR_RW(__pfx, __name) \
static struct __pfx##_attribute __pfx##_##__name##_attr = \
	__ATTR(__name, PERMS_RW, __pfx##_##__name##_show, __pfx##_##__name##_store)


/* All attributes (files in a sysfs directory) for the interface.
 * If you want to add/remove an interface, you should start here. */
#define FOREACH_IFACE_ATTR(A)\
	A(add, WO)		\
	A(uninstall, WO)	\


#define DEF_ATTR(__name, __perm)	DEF_ATTR_##__perm(iface, __name);
FOREACH_IFACE_ATTR(DEF_ATTR)
#undef DEF_ATTR

#define DEF_ATTR(__name, __perm)	&iface_##__name##_attr.attr,
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


static ssize_t dev_attr_show(struct kobject * kobj,
	struct attribute * attr, char *buf)
{
	struct vs_dev * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->show)
		return a->show(d, a, buf);

	return -EIO;
}

static ssize_t dev_attr_store(struct kobject * kobj,
	struct attribute * attr, const char * buf, size_t len)
{
	struct vs_dev * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->store)
		return a->store(d, a, buf, len);

	return -EIO;
}

static const struct sysfs_ops dev_sysfs_ops = {
	.show = dev_attr_show,
	.store = dev_attr_store,
};

static void pair_delete(struct vs_pair * pair)
{
#ifdef LOG_PAIRS
	pr_debug("%s: deleting pair %ld\n",
		pair->dev->pairs_kobj->parent->name, pair->index);
#endif

	clear_bit(pair->index, pair->dev->pairs_indexes);

	sysfs_remove_file(pair->dev->pairs_kobj, &pair->pair_file.attr);

	list_del(&pair->entry);
	kfree(pair);
}

static void clear_pairs(struct vs_dev * dev)
{
	struct list_head * e;
	struct list_head * tmp;

	list_for_each_safe(e, tmp, &dev->pair_list)
		pair_delete(list_entry(e, struct vs_pair, entry));
}

struct vs_pair * find_response(struct vs_dev * dev,
	const unsigned char * request, int req_size)
{
	return find_pair(&dev->pair_list, request, req_size);
}

static void dev_release(struct kobject *kobj)
{
	struct vs_dev * dev = to_dev(kobj);

	pr_debug("%s: releasing device\n", kobject_name(kobj));

	kfree(dev);

	pr_debug("%s: device released\n", kobject_name(kobj));
}

/*
// Stubs for the device attributes.

static ssize_t dev_show(struct vs_dev * dev,
	struct dev_attribute * attr, char * buf)
{
	return sprintf(buf, "%s/%s\n", kobject_name(&dev->kobj), attr->attr.name);
}


static ssize_t dev_store(struct vs_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	return count;
}
*/

static ssize_t dev_count_show(struct vs_dev * dev,
	struct dev_attribute * attr, char * buf)
{
	return sprintf(buf, "%d", bitmap_weight(dev->pairs_indexes, VS_MAX_PAIRS));
}

static ssize_t pair_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	const char * dev_name = kobj->parent->name;
	const char * filename = attr->attr.name;
	struct vs_iface * iface;
	struct vs_dev * dev;
	struct vs_pair * pair;
	long idx;

	if (kstrtol(filename, 0, &idx) != 0)
		return sprintf(buf, "ERROR: invalid index '%s'!", filename);

	iface = iface_from_devname(dev_name);

	if (!iface)
		return sprintf(buf, "ERROR: couldn't determine the interface for device '%s'!", dev_name);

	dev = find_device(iface, dev_name);

	if (!dev)
		return sprintf(buf, "ERROR: device '%s' not found!", dev_name);

	list_for_each_entry (pair, &dev->pair_list, entry)
		if (pair->index == idx)
			return sprintf(buf, "%s", pair_to_str(pair));

	return sprintf(buf, "ERROR: pair with index %ld not found!", idx);
}

static ssize_t dev_add_store(struct vs_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	ssize_t ret = -EINVAL;
	const char * dev_name = kobject_name(&dev->kobj);
	const char * filename = attr->attr.name;
	const char * err;
	struct vs_pair pair;
	struct vs_pair * p;
	long idx;

	err = str_to_pair(buf, count, &pair);

	if (err)
		pr_err("%s/%s: invalid request-response string: %s\n",
			dev_name, filename, err);
	else
	if ((idx = find_first_zero_bit(dev->pairs_indexes, VS_MAX_PAIRS))
	     == VS_MAX_PAIRS)
		pr_err("%s/%s: too many request-response pairs\n",
			dev_name, filename);
	else
	if (!!(p = find_pair(&dev->pair_list, pair.req, pair.req_size)))
		pr_err("%s/%s: duplicate request-response pair (%ld)\n",
			dev_name, filename, p->index);
	else
	if (!(p = kmalloc(sizeof(pair), GFP_KERNEL)))
		pr_err("%s/%s: out of memory!\n",
			dev_name, filename);
	else {
		struct kobj_attribute * f;

		memcpy(p, &pair, sizeof(pair));

		p->dev = dev;
		p->index = idx;
		snprintf(p->filename, sizeof(p->filename),
			"%ld", idx);

		f = &p->pair_file;
		f->attr.name = p->filename;
		f->attr.mode = 0444;
		f->show = pair_show;

		ret = sysfs_create_file(dev->pairs_kobj, &f->attr);

		if (ret)
			pr_err("%s/%s: sysfs_create_file() failed\n",
				dev_name, filename);
		else {
#ifdef LOG_PAIRS
			pr_debug("%s/%s: added pair %ld\n",
				dev_name, filename, idx);
#endif
			set_bit(idx, dev->pairs_indexes);
			list_add_tail(&p->entry, &dev->pair_list);
			ret = count;
		}
	}

	return ret;
}

static ssize_t dev_delete_store(struct vs_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	ssize_t ret = -EINVAL;
	const char * dev_name = kobject_name(&dev->kobj);
	const char * filename = attr->attr.name;
	struct vs_pair * pair;
	unsigned index;

	if (!count)
		pr_err("%s/%s: empty write data\n",
			dev_name, filename);
	else
	if (kstrtouint(buf, 0, &index))
		pr_err("%s/%s: invalid index value\n",
			dev_name, filename);
	else
	if (!(pair = get_pair_at_index(&dev->pair_list, index)))
		pr_err("%s/%s: no request-response pair at index %u\n",
			dev_name, filename, index);
	else {
		pair_delete(pair);
		ret = count;
	}

	return ret;
}

static ssize_t dev_clear_store(struct vs_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	if (!count)
		return -EIO;

	clear_pairs(dev);

	return count;
}

/* All attributes (files in a sysfs directory) for the device.
 * If you want to add/remove a device attribute, you should start here. */
#define FOREACH_DEV_ATTR(A)\
	A(count, RO)	\
	A(add, WO)	\
	A(delete, WO)	\
	A(clear, WO)	\

#define DEF_ATTR(__name, __perm)	DEF_ATTR_##__perm(dev, __name);
FOREACH_DEV_ATTR(DEF_ATTR)
#undef DEF_ATTR

#define DEF_ATTR(__name, __perm)	&dev_##__name##_attr.attr,
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
	struct vs_iface * ifc = &ifaces[iface];
	int err;

	INIT_LIST_HEAD(&ifc->dev_list);

	err = kobject_init_and_add(&ifc->kobj, &iface_ktype,
		&base_kset->kobj, iface_to_str(iface));

	return err;
}

static void cleanup_iface(enum VS_IFACE iface)
{
	struct vs_iface * ifc = &ifaces[iface];
	struct list_head * dl = &ifc->dev_list;

	while (!list_empty(dl)) {
		struct vs_dev * dev =
			list_first_entry(dl, struct vs_dev, entry);

		shutdown_dev(dev);
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

