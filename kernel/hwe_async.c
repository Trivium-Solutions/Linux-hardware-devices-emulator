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
	extern void hwe_##__lower##_timer_func(long jiffies);

/*! Declare timer functions for each device type. */
HWE_FOREACH_IFACE(DECL_TIMER_FUNC)

#undef DECL_TIMER_FUNC

static struct timer_list timer;

static void timer_func(struct timer_list *t)
{
	long j = jiffies;

	//pr_debug("jiffies: %lu\n", j);

#define TIMER_FUNC_CALL(__upper, __lower) hwe_##__lower##_timer_func(j),
	HWE_FOREACH_IFACE(TIMER_FUNC_CALL)
#undef TIMER_FUNC_CALL

	t->expires = j + PERIOD;
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

