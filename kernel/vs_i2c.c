#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/printk.h>

#include "vcpsim.h"


int vs_init_i2c(void)
{
	int err = 0;

	pr_info("i2c driver loaded");

	return err;
}

void vs_cleanup_i2c(void)
{

	pr_info("i2c driver unloaded");
}


