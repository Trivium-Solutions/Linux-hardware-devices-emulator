/*!
 * \file hwe_ioctl.c
 * \brief ioctl interface with the kernel module
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "hwemu.h"
#include "hwe_ioctl.h"

#define DEV_COUNT 1

static struct cdev cdev;
static dev_t dev_number = 0;
static struct class * class = NULL;
static struct device * device = NULL;

static inline long make_devid(enum HWE_IFACE iface, long index)
{
	return ((long)iface << 24) + index;
}

static inline int parse_devid(unsigned long devid, enum HWE_IFACE * iface, long * index)
{
	unsigned ifc = devid >> 24;
	unsigned idx = devid & ((1 << 24) - 1);

	if (ifc >= HWE_IFACE_COUNT || idx >= HWE_MAX_DEVICES)
		return 0;

	if (iface)
		*iface = (enum HWE_IFACE)ifc;

	if (index)
		*index = idx;

	return 1;
}

static long hwemu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static const struct file_operations hwemu_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hwemu_ioctl,
};

extern long hwe_add_device(enum HWE_IFACE iface);
extern int hwe_delete_device(enum HWE_IFACE iface, long dev_index);
extern long hwe_add_pair(enum HWE_IFACE iface, long dev_index, const char * pair_str);
extern int hwe_get_pair_count(enum HWE_IFACE iface, long dev_index);
extern int hwe_get_pair(enum HWE_IFACE iface, long dev_index, long pair_index, char * pair_str);
extern int hwe_delete_pair(enum HWE_IFACE iface, long dev_index, long pair_index);
extern int hwe_clear_pairs(enum HWE_IFACE iface, long dev_index);

static int ioctl_add_device(unsigned long arg)
{
	long idx;

	if (arg >= HWE_IFACE_COUNT)
		return -EINVAL;

	idx = hwe_add_device((enum HWE_IFACE)arg);

	if (idx < 0)
		return idx;

	return make_devid((enum HWE_IFACE)arg, idx);
}

static int ioctl_delete_device(unsigned long arg)
{
	enum HWE_IFACE ifc;
	long idx;

	if (!parse_devid(arg, &ifc, &idx))
		return -EINVAL;

	return hwe_delete_device(ifc, idx);
}

static int ioctl_pair_count(unsigned long arg)
{
	enum HWE_IFACE ifc;
	long idx;

	if (!parse_devid(arg, &ifc, &idx))
		return -EINVAL;

	return hwe_get_pair_count(ifc, idx);
}

static int ioctl_read_pair(unsigned long arg)
{
	struct hweioctl_pair __user * hp = (struct hweioctl_pair __user *)arg;
	int devid;
	enum HWE_IFACE ifc;
	long dev_idx;
	int pair_idx;
	char * pair_str;
	int ret;

	if (get_user(devid, &hp->device_id))
		return -EFAULT;

	if (!parse_devid(devid, &ifc, &dev_idx))
		return -EINVAL;

	if (get_user(pair_idx, &hp->pair_index))
		return -EFAULT;

	pair_str =  kmalloc(HWE_MAX_PAIR_STR + 1, GFP_KERNEL);

	if (!pair_str)
		return -ENOMEM;

	ret = hwe_get_pair(ifc, dev_idx, pair_idx, pair_str);

	if (ret > 0) {
		if (copy_to_user(&hp->pair, pair_str, ret + 1))
			ret = -EFAULT;
		else
			ret = 0;
	}

	kfree(pair_str);

	return ret;
}

static int ioctl_write_pair(unsigned long arg)
{
	struct hweioctl_pair __user * hp = (struct hweioctl_pair __user *)arg;
	int devid;
	enum HWE_IFACE ifc;
	long dev_idx;
	char * pair_str;
	int ret;

	if (get_user(devid, &hp->device_id))
		return -EFAULT;

	if (!parse_devid(devid, &ifc, &dev_idx))
		return -EINVAL;

	pair_str =  kmalloc(HWE_MAX_PAIR_STR + 1, GFP_KERNEL);

	if (!pair_str)
		return -ENOMEM;

	if (copy_from_user(pair_str, &hp->pair, HWE_MAX_PAIR_STR))
		return -EFAULT;

	/* terminating null, just in case */
	pair_str[HWE_MAX_PAIR_STR] = 0;

	ret = hwe_add_pair(ifc, dev_idx, pair_str);

	kfree(pair_str);

	if (ret < 0)
		return ret;

	if (put_user(ret, &hp->pair_index))
		return -EFAULT;

	return 0;
}

static int ioctl_delete_pair(unsigned long arg)
{
	struct hweioctl_pair __user * hp = (struct hweioctl_pair __user *)arg;
	int devid;
	enum HWE_IFACE ifc;
	long dev_idx;
	int pair_idx;

	if (get_user(devid, &hp->device_id))
		return -EFAULT;

	if (!parse_devid(devid, &ifc, &dev_idx))
		return -EINVAL;

	if (get_user(pair_idx, &hp->pair_index))
		return -EFAULT;

	return hwe_delete_pair(ifc, dev_idx, pair_idx);
}

static int ioctl_clear_pairs(unsigned long arg)
{
	enum HWE_IFACE ifc;
	long dev_idx;

	if (!parse_devid(arg, &ifc, &dev_idx))
		return -EINVAL;

	return hwe_clear_pairs(ifc, dev_idx);
}

static long hwemu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	pr_debug("IOCTL: cmd = 0x%x, arg = %lu\n", cmd, arg);

	switch (cmd) {
		case HWEIOCTL_ADD_DEVICE:
			err = ioctl_add_device(arg);
			break;
		case HWEIOCTL_UNINSTALL_DEVICE:
			err = ioctl_delete_device(arg);
			break;
		case HWEIOCTL_PAIR_COUNT:
			err = ioctl_pair_count(arg);
			break;
		case HWEIOCTL_READ_PAIR:
			err = ioctl_read_pair(arg);
			break;
		case HWEIOCTL_WRITE_PAIR:
			err = ioctl_write_pair(arg);
			break;
		case HWEIOCTL_DELETE_PAIR:
			err = ioctl_delete_pair(arg);
			break;
		case HWEIOCTL_CLEAR_PAIRS:
			err = ioctl_clear_pairs(arg);
			break;
		default:
			err = -ENOTTY;
	}

	return err;
}

/*! Initialize the ioctl interface with the kernel module.
*/
int hwe_init_ioctl(void)
{
	int err;

	pr_debug("initializing ioctl\n");

	err = alloc_chrdev_region(&dev_number, 0, DEV_COUNT, DRIVER_NAME);

	if (err < 0) {
		pr_err("alloc_chrdev_region() failed");
		goto quit;
	}

	class = class_create(THIS_MODULE, DRIVER_NAME);

	if (IS_ERR(class)) {
		pr_err("class_create() failed");
		err = PTR_ERR(class);
		goto err_dev;
	}

	cdev_init(&cdev, &hwemu_fops);
	cdev.owner = THIS_MODULE;

	err = cdev_add(&cdev, dev_number, DEV_COUNT);

	if (err < 0) {
		pr_err("cdev_add() failed");
		goto err_class;
	}

	device = device_create(class, NULL, dev_number, NULL, DRIVER_NAME);

	if (IS_ERR(device)) {
		pr_err("cdevice_create() failed");
		goto err_class;
	}

	pr_debug("ioctl is ready\n");
	return 0;

err_class:
	class_destroy(class);

err_dev:
	unregister_chrdev_region(dev_number, DEV_COUNT);

quit:
	return err;
}

/*! Deinitialize the ioctl interface with the kernel module.
*/
void hwe_cleanup_ioctl(void)
{
	pr_debug("deinitializing ioctl\n");

	device_destroy(class, dev_number);
	class_destroy(class);
	unregister_chrdev_region(dev_number, DEV_COUNT);

	pr_debug("ioctl is closed\n");
}
