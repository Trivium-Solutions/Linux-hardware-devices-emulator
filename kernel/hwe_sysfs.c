/*!
 * \file hwe_sysfs.c
 * \brief sysfs interface with the kernel module
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/bitmap.h>

#include "hwemu.h"

/* Comment this in, if you want to have individual pair operations
 * (add/delete/etc) logged in debug mode. */
//#define LOG_PAIRS 1

/*! \brief Internal representation of an interface */
struct hwe_iface {
	struct kobject kobj;
	struct list_head dev_list;
	struct mutex dev_mutex;
	DECLARE_BITMAP(dev_indexes, HWE_MAX_DEVICES);
};

#define to_iface(p) container_of(p, struct hwe_iface, kobj)

/*! \brief `sysfs` attribute of an interface */
struct iface_attribute {
	struct attribute attr;
	ssize_t (*show)(struct hwe_iface * iface,
		struct iface_attribute * attr, char * buf);
	ssize_t (*store)(struct hwe_iface * iface,
		struct iface_attribute * attr, const char * buf, size_t count);
};

#define to_iface_attr(p) container_of(p, struct iface_attribute, attr)

/*! \brief Internal representation of a device */
struct hwe_dev {
	struct kobject kobj;
	struct list_head entry;
	struct list_head pair_list;
	enum HWE_IFACE iface;
	long index;
	struct hwe_dev_priv * device;
	struct kobject * pairs_kobj;
	DECLARE_BITMAP(pairs_indexes, HWE_MAX_PAIRS);
};

#define to_dev(p) container_of(p, struct hwe_dev, kobj)

struct hwe_dev_priv * hwe_get_dev_priv(struct hwe_dev * dev)
{
	return dev->device;
}

enum HWE_IFACE hwe_get_dev_iface(struct hwe_dev * dev)
{
	return dev->iface;
}

long hwe_get_dev_index(struct hwe_dev * dev)
{
	return dev->index;
}

/*! \brief `sysfs` attribute of a device */
struct dev_attribute {
	struct attribute attr;
	ssize_t (*show)(struct hwe_dev * dev,
		struct dev_attribute * attr, char * buf);
	ssize_t (*store)(struct hwe_dev * dev,
		struct dev_attribute * attr, const char * buf, size_t count);
};

#define to_dev_attr(p) container_of(p, struct dev_attribute, attr)

/*! \brief Operations for each device */
struct hwe_dev_ops {
	struct hwe_dev_priv * (*create)(struct hwe_dev * dev, long index);
	void (*destroy)(struct hwe_dev_priv * device);
};

/* Prototypes for our internal device operations. */
#define DECL_DEVOP(__upper, __lower) \
	extern struct hwe_dev_priv * hwe_create_##__lower##_device(struct hwe_dev * dev, long index); \
	extern void hwe_destroy_##__lower##_device(struct hwe_dev_priv * device); \

HWE_FOREACH_IFACE(DECL_DEVOP)

#undef DECL_DEVOP

/* Internal device operations. */
#define DEVOP(__upper, __lower) { \
	.create = hwe_create_##__lower##_device, \
	.destroy = hwe_destroy_##__lower##_device, \
},

static const struct hwe_dev_ops dev_ops[] = {
	HWE_FOREACH_IFACE(DEVOP)
};

#undef DEVOP

static struct kset * base_kset;
static struct hwe_iface ifaces[HWE_IFACE_COUNT];

static ssize_t iface_attr_show(struct kobject * kobj, struct attribute * attr, char *buf)
{
	struct hwe_iface * ifc = to_iface(kobj);
	struct iface_attribute * a = to_iface_attr(attr);

	if (a->show)
		return a->show(ifc, a, buf);

	return -EIO;
}

static ssize_t iface_attr_store(struct kobject * kobj,
	struct attribute * attr, const char * buf, size_t len)
{
	struct hwe_iface * ifc = to_iface(kobj);
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

static ssize_t iface_show(struct hwe_iface * iface,
	struct iface_attribute * attr, char * buf)
{
	return sprintf(buf, "%s/%s\n", kobject_name(&iface->kobj), attr->attr.name);
}

static ssize_t iface_store(struct hwe_iface * iface,
	struct iface_attribute * attr, const char * buf, size_t count)
{
	return count;
}
*/

static inline long find_free_dev_index(enum HWE_IFACE iface)
{
	long ret = find_first_zero_bit(ifaces[iface].dev_indexes, HWE_MAX_DEVICES);

	if (ret == HWE_MAX_DEVICES)
		ret = -1;

	return ret;
}

static inline void take_dev_index(enum HWE_IFACE iface, long index)
{
	if (index >= 0)
		set_bit(index, ifaces[iface].dev_indexes);
}

static inline void put_dev_index(enum HWE_IFACE iface, long index)
{
	if (index >= 0)
		clear_bit(index, ifaces[iface].dev_indexes);
}

void lock_iface_devs(enum HWE_IFACE iface)
{
	mutex_lock(&ifaces[iface].dev_mutex);
}

void unlock_iface_devs(enum HWE_IFACE iface)
{
	mutex_unlock(&ifaces[iface].dev_mutex);
}

void lock_devs(struct hwe_dev * dev)
{
	lock_iface_devs(dev->iface);
}

void unlock_devs(struct hwe_dev * dev)
{
	unlock_iface_devs(dev->iface);
}

struct hwe_dev * find_first_device(enum HWE_IFACE iface)
{
	struct list_head * dev_list = &ifaces[iface].dev_list;

	if (list_empty(dev_list))
		return NULL;

	return list_entry(dev_list->next, struct hwe_dev, entry);
}

struct hwe_dev * find_next_device(enum HWE_IFACE iface, struct hwe_dev * device)
{
	struct list_head * dev_list = &ifaces[iface].dev_list;

	if (device->entry.next == dev_list)
		return NULL;

	return list_entry(device->entry.next, struct hwe_dev, entry);
}

struct list_head * get_pair_list(struct hwe_dev * dev)
{
	return &dev->pair_list;
}

static struct hwe_dev * find_device(enum HWE_IFACE iface, const char * name)
{
	struct hwe_dev * dev;
	struct list_head * dev_list = &ifaces[iface].dev_list;

	list_for_each_entry (dev, dev_list, entry)
		if (strcmp(kobject_name(&dev->kobj), name) == 0)
			return dev;

	return NULL;
}

static struct hwe_dev * find_device_by_index(enum HWE_IFACE iface, long index)
{
	struct hwe_dev * dev;
	struct list_head * dev_list = &ifaces[iface].dev_list;

	list_for_each_entry (dev, dev_list, entry)
		if (dev->index == index)
			return dev;

	return NULL;
}

static struct hwe_dev * add_dev(enum HWE_IFACE iface, long index)
{
	struct hwe_dev * ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (ret) {
		ret->iface = iface;
		ret->index = index;

		take_dev_index(iface, index);

		INIT_LIST_HEAD(&ret->pair_list);
		list_add(&ret->entry, &ifaces[iface].dev_list);
	}

	return ret;
}

static void del_dev(struct hwe_dev * dev)
{
	list_del(&dev->entry);

	put_dev_index(dev->iface, dev->index);

	kfree(dev);
}

static void clear_pairs(struct hwe_dev * dev);

static void shutdown_dev(struct hwe_dev * dev)
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

static struct hwe_dev * new_dev(enum HWE_IFACE iface) {
	struct hwe_iface * ifc = &ifaces[iface];
	struct hwe_dev * ret = NULL;
	long idx;
	int err;

	/* format of directory name: <interface name> <index>, eg tty0 */

	idx = find_free_dev_index(iface);

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

static ssize_t iface_add_store(struct hwe_iface * iface,
	struct iface_attribute * attr, const char * buf, size_t count)
{
	int ret = -EIO;
	const char * iface_name = kobject_name(&iface->kobj);
	const char * filename = attr->attr.name;
	enum HWE_IFACE ifc;
	struct hwe_dev * dev;

	if (count == 0)
		pr_err("%s/%s: empty write data\n",
			iface_name, filename);
	else
	if (!str_to_iface(iface_name, &ifc))
		pr_err("%s/%s: unsupported interface: %s\n",
			iface_name, filename, iface_name);
	else {
		lock_iface_devs(ifc);

		if (!(dev = new_dev(ifc)))
			pr_err("%s/%s: couldn't create new device with interface %s\n",
				iface_name, filename, iface_name);
		else {
			ret = count;

			pr_debug("%s/%s: %s: new device created\n",
				iface_name, filename, kobject_name(&dev->kobj));
		}

		unlock_iface_devs(ifc);
	}

	return ret;
}

long hwe_add_device(enum HWE_IFACE iface)
{
	struct hwe_dev * dev;
	long ret;

	lock_iface_devs(iface);

	if (!(dev = new_dev(iface)))
		ret = -ENODEV;
	else
		ret = dev->index;

	unlock_iface_devs(iface);

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

static ssize_t iface_uninstall_store(struct hwe_iface * iface,
	struct iface_attribute * attr, const char * buf, size_t count)
{
	int ret = -EINVAL;
	const char * iface_name = kobject_name(&iface->kobj);
	const char * filename = attr->attr.name;
	enum HWE_IFACE ifc;
	char dev_name[16];
	struct hwe_dev * dev;

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
	else {
		lock_iface_devs(ifc);

		if (!(dev = find_device(ifc, dev_name)))
			pr_err("%s/%s: %s: device not found\n",
				iface_name, filename, dev_name);
		else {
			pr_debug("%s/%s: %s: uninstalling device\n",
				iface_name, filename, kobject_name(&dev->kobj));

			shutdown_dev(dev);

			ret = count;
		}

		unlock_iface_devs(ifc);
	}

	return ret;
}

int hwe_delete_device(enum HWE_IFACE iface, long dev_index)
{
	struct hwe_dev * dev;
	int ret;

	lock_iface_devs(iface);

	if (!(dev = find_device_by_index(iface, dev_index)))
		ret = -ENODEV;
	else {
		shutdown_dev(dev);

		ret = 0;
	}

	unlock_iface_devs(iface);

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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
ATTRIBUTE_GROUPS(iface_default);
#endif

static struct kobj_type iface_ktype = {
	.sysfs_ops = &iface_sysfs_ops,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
	.default_groups = iface_default_groups,
#else
	.default_attrs = iface_default_attrs,
#endif
};


static ssize_t dev_attr_show(struct kobject * kobj,
	struct attribute * attr, char *buf)
{
	struct hwe_dev * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->show)
		return a->show(d, a, buf);

	return -EIO;
}

static ssize_t dev_attr_store(struct kobject * kobj,
	struct attribute * attr, const char * buf, size_t len)
{
	struct hwe_dev * d = to_dev(kobj);
	struct dev_attribute * a = to_dev_attr(attr);

	if (a->store)
		return a->store(d, a, buf, len);

	return -EIO;
}

static const struct sysfs_ops dev_sysfs_ops = {
	.show = dev_attr_show,
	.store = dev_attr_store,
};

static void pair_delete(struct hwe_pair * pair)
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

static void clear_pairs(struct hwe_dev * dev)
{
	struct list_head * e;
	struct list_head * tmp;

	list_for_each_safe(e, tmp, &dev->pair_list)
		pair_delete(list_entry(e, struct hwe_pair, entry));
}

struct hwe_pair * find_response(struct hwe_dev * dev,
	const unsigned char * request, int req_size)
{
	return find_pair(&dev->pair_list, request, req_size);
}

static void dev_release(struct kobject *kobj)
{
	struct hwe_dev * dev = to_dev(kobj);

	pr_debug("%s: releasing device\n", kobject_name(kobj));

	kfree(dev);

	pr_debug("%s: device released\n", kobject_name(kobj));
}

/*
// Stubs for the device attributes.

static ssize_t dev_show(struct hwe_dev * dev,
	struct dev_attribute * attr, char * buf)
{
	return sprintf(buf, "%s/%s\n", kobject_name(&dev->kobj), attr->attr.name);
}


static ssize_t dev_store(struct hwe_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	return count;
}
*/

static ssize_t dev_count_show(struct hwe_dev * dev,
	struct dev_attribute * attr, char * buf)
{
	ssize_t ret;

	lock_devs(dev);

	ret = sprintf(buf, "%d", bitmap_weight(dev->pairs_indexes, HWE_MAX_PAIRS));

	unlock_devs(dev);

	return ret;
}

int hwe_get_pair_count(enum HWE_IFACE iface, long dev_index)
{
	int ret;
	struct hwe_dev * dev;

	lock_iface_devs(iface);

	dev = find_device_by_index(iface, dev_index);

	if (!dev)
		ret = -ENODEV;
	else
		ret = bitmap_weight(dev->pairs_indexes, HWE_MAX_PAIRS);

	unlock_iface_devs(iface);

	return ret;
}

static ssize_t pair_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	const char * iface_name = kobj->parent->parent->name;
	const char * dev_name = kobj->parent->name;
	const char * filename = attr->attr.name;
	enum HWE_IFACE iface;
	struct hwe_dev * dev;
	struct hwe_pair * pair;
	long idx;
	ssize_t ret;

	if (kstrtol(filename, 0, &idx) != 0)
		return sprintf(buf, "ERROR: invalid index '%s'!", filename);

	if (!str_to_iface(iface_name, &iface))
		return sprintf(buf, "ERROR: invalid interface '%s'!", iface_name);

	lock_iface_devs(iface);

	dev = find_device(iface, dev_name);

	ret = 0;

	if (!dev)
		ret = sprintf(buf, "ERROR: device '%s' not found!", dev_name);
	else {
		list_for_each_entry (pair, &dev->pair_list, entry)
			if (pair->index == idx) {
				ret = sprintf(buf, "%s", pair_to_str(pair));
				break;
			}
	}

	unlock_iface_devs(iface);

	if (ret)
		return ret;

	return sprintf(buf, "ERROR: pair with index %ld not found!", idx);
}

int hwe_get_pair(enum HWE_IFACE iface, long dev_index, long pair_index,
	char * pair_str)
{
	int ret = -ENODEV;
	struct hwe_dev * dev;
	struct hwe_pair * pair;
	long idx;

	lock_iface_devs(iface);

	dev = find_device_by_index(iface, dev_index);

	if (dev) {
		ret = -ENOENT;

		list_for_each_entry (pair, &dev->pair_list, entry)
			if (pair->index == idx) {
				ret = snprintf(pair_str, HWE_MAX_PAIR_STR + 1,
					"%s", pair_to_str(pair));
				break;
			}
	}

	unlock_iface_devs(iface);

	return ret;
}

static ssize_t dev_add_store(struct hwe_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	ssize_t ret = -EINVAL;
	const char * dev_name = kobject_name(&dev->kobj);
	const char * filename = attr->attr.name;
	const char * err;
	struct hwe_pair * pair;
	struct hwe_pair * p;
	long idx;

	lock_devs(dev);

	if (!(pair = kmalloc(sizeof(*pair), GFP_KERNEL)))
		pr_err("%s/%s: out of memory!\n",
			dev_name, filename);
	else
	if (!!(err = str_to_pair(buf, count, pair)))
		pr_err("%s/%s: invalid request-response string: %s\n",
			dev_name, filename, err);
	else
	if ((idx = find_first_zero_bit(dev->pairs_indexes, HWE_MAX_PAIRS))
	     == HWE_MAX_PAIRS)
		pr_err("%s/%s: too many request-response pairs\n",
			dev_name, filename);
	else
	if (!!(p = find_pair(&dev->pair_list, pair->req, pair->req_size)))
		pr_err("%s/%s: duplicate request-response pair (%ld)\n",
			dev_name, filename, p->index);
	else {
		struct kobj_attribute * f;

		pair->dev = dev;
		pair->index = idx;
		snprintf(pair->filename, sizeof(pair->filename),
			"%ld", idx);

		f = &pair->pair_file;
		f->attr.name = pair->filename;
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
			list_add_tail(&pair->entry, &dev->pair_list);
			ret = count;
		}
	}

	if (ret < 0)
		kfree(pair);

	unlock_devs(dev);

	return ret;
}

/* FIXME This function is practically a duplicate of the above
 * dev_add_store. We need it to provide a similar IOCTL function.
 * It would be nice to somehow rewrite them both to eliminate
 * this duplicate code. */
long hwe_add_pair(enum HWE_IFACE iface, long dev_index, const char * pair_str)
{
	long ret = -EINVAL;
	const char * err;
	struct hwe_pair * pair;
	struct hwe_pair * p;
	long idx;
	struct hwe_dev * dev;

	lock_iface_devs(iface);

	if (!(dev = find_device_by_index(iface, dev_index)))
		ret = -ENODEV;
	if (!(pair = kmalloc(sizeof(*pair), GFP_KERNEL)))
		ret = -ENOMEM;
	else
	if (!!(err = str_to_pair(pair_str, strlen(pair_str), pair)))
		ret = -EINVAL;
	else
	if ((idx = find_first_zero_bit(dev->pairs_indexes, HWE_MAX_PAIRS))
	     == HWE_MAX_PAIRS)
		ret = -E2BIG;
	else
	if (!!(p = find_pair(&dev->pair_list, pair->req, pair->req_size)))
		ret = -EEXIST;
	else {
		struct kobj_attribute * f;

		pair->dev = dev;
		pair->index = idx;
		snprintf(pair->filename, sizeof(pair->filename),
			"%ld", idx);

		f = &pair->pair_file;
		f->attr.name = pair->filename;
		f->attr.mode = 0444;
		f->show = pair_show;

		ret = sysfs_create_file(dev->pairs_kobj, &f->attr);

		if (ret)
			/* error */;
		else {
			set_bit(idx, dev->pairs_indexes);
			list_add_tail(&pair->entry, &dev->pair_list);
			ret = idx;
		}
	}

	if (ret < 0)
		kfree(pair);

	unlock_iface_devs(iface);

	return ret;
}

static ssize_t dev_delete_store(struct hwe_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	ssize_t ret = -EINVAL;
	const char * dev_name = kobject_name(&dev->kobj);
	const char * filename = attr->attr.name;
	struct hwe_pair * pair;
	unsigned index;

	lock_devs(dev);

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

	unlock_devs(dev);

	return ret;
}

int hwe_delete_pair(enum HWE_IFACE iface, long dev_index, long pair_index)
{
	int ret;
	struct hwe_dev * dev;
	struct hwe_pair * pair;

	lock_iface_devs(iface);

	if (!(dev = find_device_by_index(iface, dev_index)))
		ret = -ENODEV;
	else
	if (!(pair = get_pair_at_index(&dev->pair_list, pair_index)))
		ret = -ENOENT;
	else {
		pair_delete(pair);
		ret = 0;
	}

	unlock_iface_devs(iface);

	return ret;
}

static ssize_t dev_clear_store(struct hwe_dev * dev,
	struct dev_attribute * attr, const char * buf, size_t count)
{
	if (!count)
		return -EIO;

	lock_devs(dev);

	clear_pairs(dev);

	unlock_devs(dev);

	return count;
}

int hwe_clear_pairs(enum HWE_IFACE iface, long dev_index)
{
	int ret;
	struct hwe_dev * dev;

	lock_iface_devs(iface);

	if (!(dev = find_device_by_index(iface, dev_index)))
		ret = -ENODEV;
	else {
		clear_pairs(dev);
		ret = 0;
	}

	unlock_iface_devs(iface);

	return ret;
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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
ATTRIBUTE_GROUPS(dev_default);
#endif

static struct kobj_type dev_ktype = {
	.sysfs_ops = &dev_sysfs_ops,
	.release = dev_release,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
	.default_groups = dev_default_groups,
#else
	.default_attrs = dev_default_attrs,
#endif
};


static int init_iface(enum HWE_IFACE iface)
{
	struct hwe_iface * ifc = &ifaces[iface];
	int err;

	INIT_LIST_HEAD(&ifc->dev_list);

	bitmap_zero(ifc->dev_indexes, HWE_MAX_DEVICES);

	mutex_init(&ifc->dev_mutex);

	err = kobject_init_and_add(&ifc->kobj, &iface_ktype,
		&base_kset->kobj, iface_to_str(iface));

	return err;
}

static void cleanup_iface(enum HWE_IFACE iface)
{
	struct hwe_iface * ifc = &ifaces[iface];
	struct list_head * dl = &ifc->dev_list;

	while (!list_empty(dl)) {
		struct hwe_dev * dev =
			list_first_entry(dl, struct hwe_dev, entry);

		shutdown_dev(dev);
	}

	kobject_put(&ifc->kobj);
}

/*! Initialize the sysfs interface with the kernel module.
*/
int hwe_init_sysfs(void)
{
	int i;
	int err;

	pr_debug("creating sysfs entries\n");

	base_kset = kset_create_and_add(DRIVER_NAME, NULL, kernel_kobj);

	if (!base_kset)
		return -ENOMEM;

	for (i = 0; i < HWE_IFACE_COUNT; i++) {
		err = init_iface((enum HWE_IFACE)i);
		if (err) {
			pr_err("init_iface() failed\n");

			/* clean up the interfaces installed so far */
			for (i--; i >= 0; i--)
				cleanup_iface((enum HWE_IFACE)i);

			return err;
		}
	}

	pr_info("sysfs entries created\n");
	return 0;
}

/*! Deinitialize the sysfs interface with the kernel module.
*/
void hwe_cleanup_sysfs(void)
{
	int i;

	pr_debug("cleaning up sysfs entries\n");

	for (i = HWE_IFACE_COUNT - 1; i >= 0; i--)
		cleanup_iface((enum HWE_IFACE)i);

	kset_unregister(base_kset);

	pr_info("sysfs entries cleaned up\n");
}

