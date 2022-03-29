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

struct vs_device {
	struct tty_struct * tty;
	int index;
	int open_count;
	struct mutex mutex;
	struct tty_port port;
};

static struct tty_driver * driver;
static struct vs_device * devices[VS_MAX_DEVICES];

static int vcptty_open(struct tty_struct *tty, struct file *file)
{
	struct vs_device * dev;

	tty->driver_data = NULL;

	dev = devices[tty->index];

	mutex_lock(&dev->mutex);

	tty->driver_data = dev;
	dev->tty = tty;

	dev->open_count++;
	if (dev->open_count == 1) {
		/* first time init */
		/* ... */
	}

	mutex_unlock(&dev->mutex);
	return 0;
}

static void close_dev(struct vs_device * dev)
{
	mutex_lock(&dev->mutex);

	if (!dev->open_count)
		goto quit;

	dev->open_count--;

	if (dev->open_count <= 0) {
		/* last time clean-up */
		/* ... */
	}
quit:
	mutex_unlock(&dev->mutex);
}

static void vcptty_close(struct tty_struct *tty, struct file *file)
{
	struct vs_device * dev = tty->driver_data;

	if (dev)
		close_dev(dev);
}

static int write_to_input(struct vs_device * dev, const unsigned char * buff, int count)
{
	int i;
	struct tty_port * port;

	port = &dev->port;

	for (i = 0; i < count; i++) {
		if (!tty_buffer_request_room(port, 1))
			tty_flip_buffer_push(port);

		tty_insert_flip_char(port, buff[i], TTY_NORMAL);
	}

	tty_flip_buffer_push(port);

	return count;
}

static int vcptty_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	struct vs_device * dev = tty->driver_data;
	int i;
	int ret = -EINVAL;

	if (!dev)
		return -ENODEV;

	mutex_lock(&dev->mutex);

	if (!dev->open_count)
		/* port was not opened */
		goto quit;

	/* echo */
	// XXX ---------------------------------------------------------

	write_to_input(dev, buffer, count);
	ret = count;

//	pr_debug("written %d byte(s)\n", count);

	for (i = 0; i < count; ++i) {
		if (!i)
			pr_info("write: ");
		pr_cont("%02x%c", buffer[i], (i < count - 1) ? ' ' : '\n');
	}

	// XXX ---------------------------------------------------------
quit:
	mutex_unlock(&dev->mutex);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0))
static int vcptty_write_room(struct tty_struct *tty)
#else
static unsigned int vcptty_write_room(struct tty_struct *tty)
#endif
{
	struct vs_device * dev = tty->driver_data;
	int room = -EINVAL;

	if (!dev)
		return -ENODEV;

	mutex_lock(&dev->mutex);

	if (!dev->open_count)
		goto quit;

	room = 128; /* XXX */

quit:
	mutex_unlock(&dev->mutex);
	return room;
}


static const struct tty_operations serial_ops = {
	.open = vcptty_open,
	.close = vcptty_close,
	.write = vcptty_write,
	.write_room = vcptty_write_room,
};

struct vs_device * vs_create_tty_device(long index)
{
	struct vs_device * vsdev = NULL;

	if (index < 0 || index >= VS_MAX_DEVICES)
		/* can't happen? */
		pr_err("%s%ld: device not created; index out of range!",
			iface_to_str(VS_TTY), index);
	else
	if (!(vsdev = kmalloc(sizeof(*vsdev), GFP_KERNEL)))
		pr_err("%s%ld: device not created; out of memory!",
			iface_to_str(VS_TTY), index);
	else {
		struct device * dev;

		mutex_init(&vsdev->mutex);

		vsdev->index = index;
		vsdev->open_count = 0;

		devices[index] = vsdev;

		tty_port_init(&vsdev->port);

		dev = tty_port_register_device(&vsdev->port, driver,
			index, NULL);

		if (IS_ERR(dev)) {
			tty_port_destroy(&vsdev->port);
			kfree(vsdev);
			devices[index] = vsdev = NULL;
			pr_err("%s%ld: device not created; "
				"tty_port_register_device() error code "
				"%ld\n", iface_to_str(VS_TTY), index,
				PTR_ERR(dev));
		}
	}

	return vsdev;
}

void vs_destroy_tty_device(struct vs_device * device)
{
	int idx = device->index;

	tty_unregister_device(driver, idx);
	tty_port_destroy(&device->port);
	kfree(device);
	devices[idx] = NULL;
}

int vs_init_tty(void)
{
	int err;

	pr_debug("loading tty driver\n");

	driver = tty_alloc_driver(VS_MAX_DEVICES,
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV);

	if (!driver)
		return -ENOMEM;

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
		if (devices[i])
			pr_err("%s%d was not destroyed!\n",
				iface_to_str(VS_TTY), i);

	tty_unregister_driver(driver);
	tty_driver_kref_put(driver);

	pr_info("tty driver unloaded\n");
}

