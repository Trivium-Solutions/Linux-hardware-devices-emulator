/*!
 * \file hwe_spi.c
 * \brief SPI device emulator
 *
 */
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#include <linux/of.h>
#include <linux/platform_device.h>

#include "hwemu.h"

/*! Private data for the SPI device */
struct hwe_dev_priv {
	struct hwe_dev * hwedev;
	struct list_head devices;
};

static struct list_head devices;

/*! Create an instance of the SPI device.
 */
struct hwe_dev_priv * hwe_create_spi_device(struct hwe_dev * hwedev, long index)
{
	struct hwe_dev_priv *priv = (void*)1; /* XXX temporary; must be non-null */
	int err;


	return priv;
}

/*! Destroy an instance of the SPI device.
 */
void hwe_destroy_spi_device(struct hwe_dev_priv * device)
{

}

/*! Initialize the SPI device emulator.
 */
int hwe_init_spi(void)
{
	int err = 0;

	pr_debug("loading spi driver\n");

	INIT_LIST_HEAD(&devices);

	pr_info("spi driver loaded\n");

	return err;
}

/*! Deinitialize the SPI device emulator.
 */
void hwe_cleanup_spi(void)
{
	struct list_head * e;
	struct list_head * tmp;

	list_for_each_safe(e, tmp, &devices) {
		struct hwe_dev_priv * dev = list_entry(e, struct hwe_dev_priv, devices);

		hwe_destroy_spi_device(dev);
	}

	pr_info("spi driver unloaded\n");
}

