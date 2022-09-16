/*!
 * \file hwe_net.c
 * \brief Network device emulator
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>

#include "hwemu.h"

#define	NET_DRIVER_NAME	"hwenet"

/*! Private data for the network device */
struct hwe_dev_priv {
	struct hwe_dev * hwedev;
	long index;
	struct net_device *net_dev;
	struct list_head devices;
};

static struct list_head devices;

static int hwenet_open(struct net_device *ndev)
{
	netif_start_queue(ndev);
	return 0;
}

static int hwenet_stop(struct net_device *ndev)
{
	netif_stop_queue(ndev);
	return 0;
}

static int send_response(struct net_device *dev, const char * data, unsigned int len)
{
	struct sk_buff *skb = dev_alloc_skb(len + 2);

	skb_reserve(skb, 2);
	memcpy(skb_put(skb, len), data, len);
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	dev->stats.rx_packets++;
	return netif_rx(skb);
}

static int hwenet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct hwe_dev_priv *priv = netdev_priv(ndev);
	struct hwe_pair * pair;

	ndev->stats.tx_bytes += skb->len;
	ndev->stats.tx_packets++;
	skb_tx_timestamp(skb);

	pair = find_response(priv->hwedev, skb->data, skb->len);
	hwe_log_request(HWE_NET, priv->index, skb->data, skb->len, !!pair);

	dev_kfree_skb(skb);

	if (pair) {
		hwe_log_response(HWE_NET, priv->index, pair->resp, pair->resp_size);
		return send_response(ndev, pair->resp, pair->resp_size);
	}

	return NETDEV_TX_OK;
}

static const struct net_device_ops hwe_netdev_ops = {
	.ndo_open = hwenet_open,
	.ndo_stop = hwenet_stop,
	.ndo_start_xmit = hwenet_xmit,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_set_mac_address = eth_mac_addr,
};

static void hwenet_init(struct net_device *ndev)
{
	struct hwe_dev_priv *priv;

	eth_hw_addr_random(ndev);

	ether_setup(ndev);

	ndev->netdev_ops = &hwe_netdev_ops;
	ndev->flags |= IFF_NOARP;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 10, 0))
	ndev->max_mtu = 4 * 1024;
#endif
	ndev->features |= NETIF_F_HW_CSUM;

	priv = netdev_priv(ndev);

	priv->net_dev = ndev;
	list_add(&priv->devices, &devices);
}

/*! Create an instance of the network device.
 */
struct hwe_dev_priv * hwe_create_net_device(struct hwe_dev * hwedev, long index)
{
	struct net_device *ndev;
	struct hwe_dev_priv *priv;
	int err;

	ndev = alloc_netdev(sizeof(struct hwe_dev_priv),
		NET_DRIVER_NAME"%d", NET_NAME_UNKNOWN, hwenet_init);

	if (!ndev) {
		pr_err("%s%ld: alloc_netdev() failed\n", NET_DRIVER_NAME,
			index);
		return NULL;
	}

	priv = netdev_priv(ndev);

	priv->index = index;
	priv->hwedev = hwedev;

	err = register_netdev(ndev);

	if (err) {
		pr_err("%s%ld: register_netdev() failed (error code %d)\n",
			NET_DRIVER_NAME, index, err);
		list_del(&priv->devices);
		free_netdev(ndev);
		return NULL;
	}

	return priv;
}

/*! Destroy an instance of the network device.
 */
void hwe_destroy_net_device(struct hwe_dev_priv * device)
{
	list_del(&device->devices);
	unregister_netdev(device->net_dev);
	free_netdev(device->net_dev);
}

/*! Initialize the network device emulator.
 */
int hwe_init_net(void)
{
	int err = 0;

	pr_debug("loading net driver\n");

	INIT_LIST_HEAD(&devices);

	pr_info("net driver loaded\n");

	return err;
}

/*! Deinitialize the network device emulator.
 */
void hwe_cleanup_net(void)
{
	struct list_head * e;
	struct list_head * tmp;

	list_for_each_safe(e, tmp, &devices) {
		struct hwe_dev_priv * dev = list_entry(e, struct hwe_dev_priv, devices);

		hwe_destroy_net_device(dev);
	}

	pr_info("net driver unloaded\n");
}

void hwe_net_timer_func(long jiffies)
{

}
