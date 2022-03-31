
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
static int (* const init_funcs[])(void) = {
	VS_FOREACH_IFACE(INIT_FUNC_PTR)
	vs_init_sysfs,
};

/*! Array of pointers to various clean-up functions
   that must be called at unload time. The functions
   must be called in reverse order! */
static void (* const cleanup_funcs[])(void) = {
	VS_FOREACH_IFACE(CLEANUP_FUNC_PTR)
	vs_cleanup_sysfs,
};

#undef INIT_FUNC
#undef CLEANUP_FUNC
#undef DECL_INIT_FUNC
#undef DECL_CLEANUP_FUNC

static bool log_requests = false;
static bool log_responses = false;

/*! Write request to kernel log */
void vs_log_request(enum VS_IFACE iface, long dev_num,
	const void * request, size_t req_size, bool have_response)
{
	if (!log_requests)
		return;

	pr_info("%s%ld <-- %ld byte(s) (response %savailable):\n",
		iface_to_str(iface), dev_num, req_size,
		have_response ? "" : "not ");

	print_hex_dump(KERN_INFO, "", 0, 16, 1, request, req_size, false);
}

/*! Write response to kernel log */
void vs_log_response(enum VS_IFACE iface, long dev_num,
	const void * response, size_t resp_size)
{
	if (!log_responses)
		return;

	pr_info("%s%ld --> %ld byte(s):\n",
		iface_to_str(iface), dev_num, resp_size);

	print_hex_dump(KERN_INFO, "", 0, 16, 1, response, resp_size, false);
}

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

module_param(log_requests, bool, 0644);
MODULE_PARM_DESC(log_requests, "Enable logging of requests");

module_param(log_responses, bool, 0644);
MODULE_PARM_DESC(log_responses, "Enable logging of responses");
