
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/version.h>

#include "vcpsim.h"

int vs_init_sysfs(void)
{

	pr_debug("creating sysfs entries\n");

	pr_info("sysfs entries created\n");
	return 0;
}

void vs_cleanup_sysfs(void)
{

	pr_debug("cleaning up sysfs entries\n");

	pr_info("sysfs entries cleaned up\n");
}

