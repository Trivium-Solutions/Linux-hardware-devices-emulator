/*!
 * \file hwe_async.c
 * \brief Asynchronous interactions
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/uaccess.h>

#include "hwemu.h"

extern int hwe_init_async(void)
{
	int err = 0;

	pr_debug("initializing async\n");

	pr_debug("async is ready\n");

	return err;
}

extern void hwe_cleanup_async(void)
{
	pr_debug("deinitializing async\n");


	pr_debug("async is closed\n");
}

