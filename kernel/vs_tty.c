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

struct vs_device * vs_create_tty_device(long index)
{
	struct vs_device * vsdev = (struct vs_device *)(index + 1); /* XXX */

	pr_debug("%s%ld: dummy device created\n",
		iface_to_str(VS_TTY), index);

	return vsdev;
}

void vs_destroy_tty_device(struct vs_device * device)
{
	pr_debug("%s%ld: dummy device destroyed\n",
		iface_to_str(VS_TTY), (long)device - 1);
}

int vs_init_tty(void)
{
	int err = 0;

	pr_info("tty driver loaded\n");

	return err;
}


void vs_cleanup_tty(void)
{

	pr_info("tty driver unloaded\n");
}


