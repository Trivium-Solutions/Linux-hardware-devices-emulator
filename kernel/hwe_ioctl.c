/*!
 * \file hwe_ioctl.c
 * \brief ioctl interface with the kernel module
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
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

static long hwemu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static const struct file_operations hwemu_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hwemu_ioctl,
};

static long hwemu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	pr_debug("IOCTL: cmd = 0x%x, arg = %lu\n", cmd, arg);

	switch (cmd) {
		case HWEIOCTL_ADD_DEVICE:
			break;
		case HWEIOCTL_UNINSTALL_DEVICE:
			break;
		case HWEIOCTL_COUNT:
			break;
		case HWEIOCTL_READ_PAIR:
			break;
		case HWEIOCTL_WRITE_PAIR:
			break;
		case HWEIOCTL_DELETE:
			break;
		case HWEIOCTL_CLEAR:
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
