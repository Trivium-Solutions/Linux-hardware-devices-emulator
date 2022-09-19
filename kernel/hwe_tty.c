/*!
 * \file hwe_tty.c
 * \brief TTY device emulator
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "hwemu.h"

#define TTY_DRIVER_NAME "hwetty"

/*! Private data for the TTY device. */
struct hwe_dev_priv {
	struct hwe_dev * hwedev;
	struct tty_struct * tty;
	int index;
};

static struct tty_driver * driver;
static struct hwe_dev_priv * devices[HWE_MAX_DEVICES];
static struct tty_port ports[HWE_MAX_DEVICES];

#define NODEV_ERROR ENODEV

static int hwetty_open(struct tty_struct *tty, struct file *file)
{
	int err = -NODEV_ERROR;
	struct hwe_dev_priv * dev;

	/* We assume that at this point:
	 *
	 * 1. multiple processes may be trying to open this device
	 *    simultaneously;
	 * 2. the device may have already been removed by the user;
	 * 3. the list of request-response pairs may be changed during
	 *    the operation.
	 *
	 * These assumptions are also valid for the other file operations. */

	lock_iface_devs(HWE_TTY);

	tty->driver_data = &devices[tty->index];

	dev = *(struct hwe_dev_priv **)tty->driver_data;

	if (!dev)
		goto quit;

	dev->tty = tty;

	err = 0;
quit:
	unlock_iface_devs(HWE_TTY);

	return err;
}

static void close_dev(struct hwe_dev_priv * dev)
{
	/* any specific action? */
}

static void hwetty_close(struct tty_struct *tty, struct file *file)
{
	struct hwe_dev_priv * dev;

	lock_iface_devs(HWE_TTY);

	dev = *(struct hwe_dev_priv **)tty->driver_data;

	if (dev)
		close_dev(dev);

	unlock_iface_devs(HWE_TTY);
}

static int hwetty_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	int ret = -NODEV_ERROR;
	struct hwe_dev_priv * dev;
	struct hwe_pair * pair;

	lock_iface_devs(HWE_TTY);

	dev = *(struct hwe_dev_priv **)tty->driver_data;

	if (!dev)
		goto quit;

	pair = find_response(dev->hwedev, buffer, count);

	if (pair) {
		int n = tty_insert_flip_string_fixed_flag(&ports[dev->index],
			pair->resp, TTY_NORMAL, pair->resp_size);

		tty_flip_buffer_push(&ports[dev->index]);

		if (n != pair->resp_size)
			pr_err("tty_insert_flip_string_fixed_flag() "
				"added only %d byte(s) of %ld\n",
				n, (long)pair->resp_size);
	}

	hwe_log_request(HWE_TTY, dev->index, buffer, count, !!pair);

	if (pair)
		hwe_log_response(HWE_TTY, dev->index, pair->resp, pair->resp_size);

	ret = count;
quit:
	unlock_iface_devs(HWE_TTY);

	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0))
static int hwetty_write_room(struct tty_struct *tty)
#else
static unsigned int hwetty_write_room(struct tty_struct *tty)
#endif
{
	struct hwe_dev_priv * dev;
	int room = -NODEV_ERROR;

	lock_iface_devs(HWE_TTY);

	dev = *(struct hwe_dev_priv **)tty->driver_data;

	if (!dev)
		goto quit;

	room = PAGE_SIZE;

quit:
	unlock_iface_devs(HWE_TTY);

	return room;
}


static const struct tty_operations serial_ops = {
	.open = hwetty_open,
	.close = hwetty_close,
	.write = hwetty_write,
	.write_room = hwetty_write_room,
};

/*! Create an instance of the TTY device.
 */
struct hwe_dev_priv * hwe_create_tty_device(struct hwe_dev * hwedev, long index)
{
	struct hwe_dev_priv * dev = NULL;

	if (index < 0 || index >= HWE_MAX_DEVICES)
		/* can't happen? */
		pr_err("%s%ld: device not created; index out of range!\n",
			iface_to_str(HWE_TTY), index);
	else
	if (!(dev = kzalloc(sizeof(*dev), GFP_KERNEL)))
		pr_err("%s%ld: device not created; out of memory!\n",
			iface_to_str(HWE_TTY), index);
	else {
		struct device * d;

		dev->hwedev = hwedev;
		dev->index = index;

		devices[index] = dev;

		d = tty_port_register_device(&ports[index], driver,
			index, NULL);

		if (IS_ERR(d)) {
			kfree(dev);
			devices[index] = dev = NULL;
			pr_err("%s%ld: device not created; "
				"tty_port_register_device() error code "
				"%ld\n", iface_to_str(HWE_TTY), index,
				PTR_ERR(d));
		}
	}

	return dev;
}

/*! Destroy an instance of the TTY device.
 */
void hwe_destroy_tty_device(struct hwe_dev_priv * device)
{
	int idx = device->index;

	tty_unregister_device(driver, idx);
	kfree(device);
	devices[idx] = NULL;
}

/*! Initialize the TTY emulator.
 */
int hwe_init_tty(void)
{
	int i;
	int err;

	pr_debug("loading tty driver\n");

	driver = tty_alloc_driver(HWE_MAX_DEVICES,
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV);

	if (!driver)
		return -ENOMEM;

	for (i = 0; i < HWE_MAX_DEVICES; i++)
		tty_port_init(&ports[i]);

	driver->owner = THIS_MODULE;
	driver->driver_name = TTY_DRIVER_NAME;
	driver->name = "ttyHWE";
	driver->major = 0; /* allocate dynamically */
	driver->type = TTY_DRIVER_TYPE_SERIAL;
	driver->subtype = SERIAL_TYPE_NORMAL;
	driver->init_termios = tty_std_termios;
	driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	driver->init_termios.c_lflag &= ~ECHO;

	tty_set_operations(driver, &serial_ops);

	err = tty_register_driver(driver);

	if (err) {
		pr_err("failed to register " TTY_DRIVER_NAME " driver\n");
		tty_driver_kref_put(driver);

		for (i = 0; i < HWE_MAX_DEVICES; i++)
			tty_port_destroy(&ports[i]);

		return err;
	}

	pr_info("tty driver loaded\n");

	return 0;
}

/*! Deinitialize the TTY emulator.
 */
void hwe_cleanup_tty(void)
{
	int i;

	/* sanity check */
	for (i = 0; i < HWE_MAX_DEVICES; i++)
		if (devices[i]) {
			pr_err("%s%d was not destroyed before driver unload!\n",
				iface_to_str(HWE_TTY), i);
			hwe_destroy_tty_device(devices[i]);
		}

	tty_unregister_driver(driver);
	tty_driver_kref_put(driver);

	for (i = 0; i < HWE_MAX_DEVICES; i++)
		tty_port_destroy(&ports[i]);

	pr_info("tty driver unloaded\n");
}

void hwe_tty_timer_func(long jiffies)
{

}
