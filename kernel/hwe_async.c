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

#define PERIOD (5 * HZ)

#define DECL_TIMER_FUNC(__upper, __lower) \
	extern void hwe_##__lower##_async_rx(struct hwe_dev * device, struct hwe_pair * pair);

/*! Declare timer functions for each device type. */
HWE_FOREACH_IFACE(DECL_TIMER_FUNC)

#undef DECL_TIMER_FUNC

/*! Array of asynchronous reception handlers for each interface. */
static void (* const async_rx[])(struct hwe_dev * device, struct hwe_pair * pair) = {
#define FUNC_PTR(__upper, __lower) hwe_##__lower##_async_rx,
	HWE_FOREACH_IFACE(FUNC_PTR)
#undef FUNC_PTR
};

static struct timer_list timer;

static void timer_func(struct timer_list *t)
{
	long jiff = jiffies;
	enum HWE_IFACE ifc;

	//pr_debug("jiffies: %lu\n", jiff);

	for (ifc = 0; ifc < HWE_IFACE_COUNT; ifc++) {
		struct hwe_dev * dev;

		lock_iface_devs(ifc);

		dev = find_first_device(ifc);

		while (dev) {
			struct list_head * pairs = get_pair_list(dev);
			struct hwe_pair * p;

			list_for_each_entry (p, pairs, entry) {

//				async_rx[ifc](dev, p);
			}

			dev = find_next_device(ifc, dev);
		}

		unlock_iface_devs(ifc);
	}

	t->expires = jiffies + PERIOD;
	add_timer(t);
}

extern int hwe_init_async(void)
{
	int err = 0;

	pr_debug("initializing async\n");
	pr_debug(" HZ == %d\n", HZ);

	timer_setup(&timer, timer_func, 0);
	timer.expires = jiffies + PERIOD;
	add_timer(&timer);

	pr_debug("async is ready\n");

	return err;
}

extern void hwe_cleanup_async(void)
{
	pr_debug("deinitializing async\n");

	del_timer(&timer);

	pr_debug("async is closed\n");
}

