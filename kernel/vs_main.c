
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>

#include "vcpsim.h"

#define DECL_INIT_FUNC(__upper, __lower) \
	extern int vs_init_##__lower(void);
#define DECL_CLEANUP_FUNC(__upper, __lower) \
	extern void vs_cleanup_##__lower(void);

#define INIT_FUNC_PTR(__upper, __lower) vs_init_##__lower,
#define CLEANUP_FUNC_PTR(__upper, __lower) vs_cleanup_##__lower,

extern int vs_init_sysfs(void);
extern void vs_cleanup_sysfs(void);

/*! Declare init functions for each device type. */
VS_FOREACH_IFACE(DECL_INIT_FUNC)

/*! Declare clean-up functions for each device type. */
VS_FOREACH_IFACE(DECL_CLEANUP_FUNC)

/*! Array of pointers to various initialization functions
   that must be called at load time. */
static int (* init_funcs[])(void) = {
	vs_init_sysfs,
	VS_FOREACH_IFACE(INIT_FUNC_PTR)
};

/*! Array of pointers to various clean-up functions
   that must be called at unload time. The functions
   must be called in reverse order! */
static void (* cleanup_funcs[])(void) = {
	vs_cleanup_sysfs,
	VS_FOREACH_IFACE(CLEANUP_FUNC_PTR)
};

#undef INIT_FUNC
#undef CLEANUP_FUNC
#undef DECL_INIT_FUNC
#undef DECL_CLEANUP_FUNC

static int __init vcpsim_init(void)
{
	int err;
	int i;

	for (i = 0; i < ARRAY_SIZE(init_funcs); i++)
		if (!!(err = init_funcs[i]()))
			goto err_init_funcs;

	pr_info("loaded\n");
	return 0;

err_init_funcs:

	for (i--; i >= 0; i--)
		cleanup_funcs[i]();

	return err;
}

static void __exit vcpsim_exit(void) {
	int i;

	for (i = ARRAY_SIZE(cleanup_funcs) - 1; i >= 0; i--)
		cleanup_funcs[i]();

	pr_info("unloaded\n");
}

module_init(vcpsim_init);
module_exit(vcpsim_exit);

MODULE_LICENSE("GPL");

