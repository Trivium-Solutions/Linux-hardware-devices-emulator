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
	struct spi_master *master;
	struct spi_device *spi_dev;
};

static struct list_head devices;
static struct platform_device * plat_device;

static int hwespi_transfer_one(struct spi_controller *ctlr, struct spi_device *spi,
			    struct spi_transfer *transfer)
{
	pr_debug("%s()\n", __func__);
	pr_debug(" transfer->len == %u\n", transfer->len);
	pr_debug(" transfer->rx_buf == %p\n", transfer->rx_buf);
	pr_debug(" transfer->tx_buf == %p\n", transfer->tx_buf);

	if (transfer->rx_buf) {
		unsigned char * rx_buf = transfer->rx_buf;
		const unsigned char * tx_buf = transfer->tx_buf;
		int i;

		for (i = 0; i < transfer->len; i++)
			rx_buf[i] = tx_buf ? ~tx_buf[i] : 0;

	}

	spi_finalize_current_transfer(ctlr);

	return 0;
}

struct spi_board_info chip = {
//	.modalias = "hwe_spi",
	/* One of the devices supported by spidev. Here we use a fake
	 * device to skip the procedure of binding our device to spidev.
	 * See https://docs.kernel.org/spi/spidev.html */
	.modalias = "bk4",
};

static struct hwe_dev_priv * new_dev(struct hwe_dev * hwedev, struct platform_device *pdev)
{
	struct hwe_dev_priv * ret = kzalloc(sizeof(*ret), GFP_KERNEL);
	int err;

	if (!ret) {
		pr_err("kzalloc() failed!\n");
		return NULL;
	}

	ret->hwedev = hwedev;

	ret->master = spi_alloc_master(&pdev->dev, 0);

	if (ret->master == NULL) {
		pr_err("spi_alloc_master() failed\n");
		return NULL;
	}

	ret->master->num_chipselect = 1;

	ret->master->transfer_one = hwespi_transfer_one;

	err = spi_register_master(ret->master);

	if (err) {
		pr_err("spi_register_master() failed (%d)\n", err);
		spi_master_put(ret->master);
		kfree(ret);
		return NULL;
	}

	ret->spi_dev = spi_new_device(ret->master, &chip);

	if (!ret->spi_dev) {
		pr_err("spi_new_device() failed\n");
		spi_master_put(ret->master);
		kfree(ret);
		return NULL;
	}

	list_add(&ret->devices, &devices);

	return ret;
}

void del_dev(struct hwe_dev_priv * device)
{
	list_del(&device->devices);

	spi_unregister_device(device->spi_dev);
	spi_unregister_master(device->master);
	kfree(device);
}

static int plat_probe(struct platform_device *pdev)
{
	int err = 0;

	/* STUB */

	return err;
}

static int plat_remove(struct platform_device *pdev)
{

	/* STUB */
	return 0;
}

static struct platform_driver plat_driver = {
	.driver = {
		.name	= "hwe_plat",
		.owner  = THIS_MODULE,
	},
	.probe		= plat_probe,
	.remove		= plat_remove,
};

/*! Create an instance of the SPI device.
 */
struct hwe_dev_priv * hwe_create_spi_device(struct hwe_dev * hwedev, long index)
{
	struct hwe_dev_priv *priv;

	priv = new_dev(hwedev, plat_device);

	return priv;
}

/*! Destroy an instance of the SPI device.
 */
void hwe_destroy_spi_device(struct hwe_dev_priv * device)
{
	del_dev(device);
}

/*! Initialize the SPI device emulator.
 */
int hwe_init_spi(void)
{
	int err = 0;

	pr_debug("loading spi driver\n");

	INIT_LIST_HEAD(&devices);

	plat_device =  platform_device_register_simple("hwe_plat",
		PLATFORM_DEVID_NONE, NULL, 0);

	if (IS_ERR(plat_device)) {
		pr_err("platform_device_register_simple() failed\n");
		return PTR_ERR(plat_device);
	}

	err =  platform_driver_register(&plat_driver);

	if (err) {
		platform_device_unregister(plat_device);
		pr_err("platform_driver_register() failed\n");
		return err;
	}

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

	platform_driver_unregister(&plat_driver);
	platform_device_unregister(plat_device);

	pr_info("spi driver unloaded\n");
}

