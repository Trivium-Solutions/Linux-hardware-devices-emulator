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

int vs_init_tty(void)
{
	int err = 0;

	pr_info("tty driver loaded");

	return err;
}


void vs_cleanup_tty(void)
{

	pr_info("tty driver unloaded");
}


