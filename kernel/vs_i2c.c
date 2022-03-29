#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/printk.h>

#include "vcpsim.h"


struct vs_dev_priv * vs_create_i2c_device(struct vs_dev * vsdev, long index)
{
	struct vs_dev_priv * dev = (struct vs_dev_priv *)(index + 1); /* XXX */

	pr_debug("%s%ld: dummy device created\n",
		iface_to_str(VS_I2C), index);

	return dev;
}

void vs_destroy_i2c_device(struct vs_dev_priv * device)
{
	pr_debug("%s%ld: dummy device destroyed\n",
		iface_to_str(VS_I2C), (long)device - 1);
}

int vs_init_i2c(void)
{
	int err = 0;

	pr_debug("loading i2c driver\n");

	pr_info("i2c driver loaded\n");

	return err;
}

void vs_cleanup_i2c(void)
{

	pr_info("i2c driver unloaded\n");
}


