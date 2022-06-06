#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>

#include "hwemu.h"

#define	NET_DRIVER_NAME	"hwenet"

struct hwe_dev_priv {
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

static int hwenet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct hwe_dev_priv *priv = netdev_priv(ndev);

//	pr_info("%s%ld:\n", NET_DRIVER_NAME, priv->index);
//	print_hex_dump(KERN_INFO, " tx: ", 0, 16, 1, skb->data, skb->len, 1);

	ndev->stats.tx_bytes += skb->len;
	ndev->stats.tx_packets++;
	skb_tx_timestamp(skb);
	dev_kfree_skb(skb);

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
	ndev->max_mtu = 4 * 1024;
//	ndev->features |= NETIF_F_HW_CSUM;

	priv = netdev_priv(ndev);

	priv->net_dev = ndev;
	list_add(&priv->devices, &devices);
}

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

void hwe_destroy_net_device(struct hwe_dev_priv * device)
{
	list_del(&device->devices);
	unregister_netdev(device->net_dev);
	free_netdev(device->net_dev);
}

int hwe_init_net(void)
{
	int err = 0;

	pr_debug("loading net driver\n");

	INIT_LIST_HEAD(&devices);

	pr_info("net driver loaded\n");

	return err;
}

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

