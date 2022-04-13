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

#include "vcpsim.h"

#define TTY_DRIVER_NAME "vcptty"

struct vs_dev_priv {
	struct vs_dev * vsdev;
	struct tty_struct * tty;
	int index;
};

static struct tty_driver * driver;
static struct vs_dev_priv * devices[VS_MAX_DEVICES];
static struct tty_port ports[VS_MAX_DEVICES];

#define NODEV_ERROR ENODEV

static int vcptty_open(struct tty_struct *tty, struct file *file)
{
	int err = -NODEV_ERROR;
	struct vs_dev_priv * dev;

	/* We assume that at this point:
	 *
	 * 1. multiple processes may be trying to open this device
	 *    simultaneously;
	 * 2. the device may have already been removed by the user;
	 * 3. the list of request-response pairs may be changed during
	 *    the operation.
	 *
	 * These assumptions are also valid for the other file operations. */

	lock_iface_devs(VS_TTY);

	tty->driver_data = &devices[tty->index];

	dev = *(struct vs_dev_priv **)tty->driver_data;

	if (!dev)
		goto quit;

	dev->tty = tty;

	err = 0;
quit:
	unlock_iface_devs(VS_TTY);

	return err;
}

static void close_dev(struct vs_dev_priv * dev)
{
	/* any specific action? */
}

static void vcptty_close(struct tty_struct *tty, struct file *file)
{
	struct vs_dev_priv * dev;

	lock_iface_devs(VS_TTY);

	dev = *(struct vs_dev_priv **)tty->driver_data;

	if (dev)
		close_dev(dev);

	unlock_iface_devs(VS_TTY);
}

static int vcptty_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	int ret = -NODEV_ERROR;
	struct vs_dev_priv * dev;
	struct vs_pair * pair;

	lock_iface_devs(VS_TTY);

	dev = *(struct vs_dev_priv **)tty->driver_data;

	if (!dev)
		goto quit;

	pair = find_response(dev->vsdev, buffer, count);

	if (pair) {
		int n = tty_insert_flip_string_fixed_flag(&ports[dev->index],
			pair->resp, TTY_NORMAL, pair->resp_size);

		tty_flip_buffer_push(&ports[dev->index]);

		if (n != pair->resp_size)
			pr_err("tty_insert_flip_string_fixed_flag() "
				"added only %d byte(s) of %ld\n",
				n, pair->resp_size);
	}

	vs_log_request(VS_TTY, dev->index, buffer, count, !!pair);

	if (pair)
		vs_log_response(VS_TTY, dev->index, pair->resp, pair->resp_size);

	ret = count;
quit:
	unlock_iface_devs(VS_TTY);

	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0))
static int vcptty_write_room(struct tty_struct *tty)
#else
static unsigned int vcptty_write_room(struct tty_struct *tty)
#endif
{
	struct vs_dev_priv * dev;
	int room = -NODEV_ERROR;

	lock_iface_devs(VS_TTY);

	dev = *(struct vs_dev_priv **)tty->driver_data;

	if (!dev)
		goto quit;

	room = PAGE_SIZE;

quit:
	unlock_iface_devs(VS_TTY);

	return room;
}


static const struct tty_operations serial_ops = {
	.open = vcptty_open,
	.close = vcptty_close,
	.write = vcptty_write,
	.write_room = vcptty_write_room,
};

struct vs_dev_priv * vs_create_tty_device(struct vs_dev * vsdev, long index)
{
	struct vs_dev_priv * dev = NULL;

	if (index < 0 || index >= VS_MAX_DEVICES)
		/* can't happen? */
		pr_err("%s%ld: device not created; index out of range!",
			iface_to_str(VS_TTY), index);
	else
	if (!(dev = kzalloc(sizeof(*dev), GFP_KERNEL)))
		pr_err("%s%ld: device not created; out of memory!",
			iface_to_str(VS_TTY), index);
	else {
		struct device * d;

		dev->vsdev = vsdev;
		dev->index = index;

		devices[index] = dev;

		d = tty_port_register_device(&ports[index], driver,
			index, NULL);

		if (IS_ERR(d)) {
			kfree(dev);
			devices[index] = dev = NULL;
			pr_err("%s%ld: device not created; "
				"tty_port_register_device() error code "
				"%ld\n", iface_to_str(VS_TTY), index,
				PTR_ERR(d));
		}
	}

	return dev;
}

void vs_destroy_tty_device(struct vs_dev_priv * device)
{
	int idx = device->index;

	tty_unregister_device(driver, idx);
	kfree(device);
	devices[idx] = NULL;
}

int vs_init_tty(void)
{
	int i;
	int err;

	pr_debug("loading tty driver\n");

	driver = tty_alloc_driver(VS_MAX_DEVICES,
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV);

	if (!driver)
		return -ENOMEM;

	for (i = 0; i < VS_MAX_DEVICES; i++)
		tty_port_init(&ports[i]);

	driver->owner = THIS_MODULE;
	driver->driver_name = TTY_DRIVER_NAME;
	driver->name = "ttyVCP";
	driver->major = 0; /* allocate dynamically */
	driver->type = TTY_DRIVER_TYPE_SERIAL;
	driver->subtype = SERIAL_TYPE_NORMAL;
	driver->init_termios = tty_std_termios;
	driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	driver->init_termios.c_lflag &= ~ECHO;

	tty_set_operations(driver, &serial_ops);

	err = tty_register_driver(driver);

	if (err) {
		pr_err("failed to register " TTY_DRIVER_NAME " driver");
		tty_driver_kref_put(driver);

		for (i = 0; i < VS_MAX_DEVICES; i++)
			tty_port_destroy(&ports[i]);

		return err;
	}

	pr_info("tty driver loaded\n");

	return 0;
}

void vs_cleanup_tty(void)
{
	int i;


	/* sanity check */
	for (i = 0; i < VS_MAX_DEVICES; i++)
		if (devices[i]) {
			pr_err("%s%d was not destroyed before driver unload!\n",
				iface_to_str(VS_TTY), i);
			vs_destroy_tty_device(devices[i]);
		}

	tty_unregister_driver(driver);
	tty_driver_kref_put(driver);

	for (i = 0; i < VS_MAX_DEVICES; i++)
		tty_port_destroy(&ports[i]);

	pr_info("tty driver unloaded\n");
}

