#ifdef __KERNEL__
#	include <linux/kernel.h>
#	include <linux/string.h>
#	include <linux/ctype.h>
#else
#	include <stdio.h>
#	include <string.h>
#	include <ctype.h>
#endif

#include "vcpsim.h"


/*! Returns a string with the name of iface. */
const char * iface_to_str(enum VS_IFACE iface)
{
	switch (iface) {
#define DO_CASE(__upper, __lower) case VS_##__upper: return #__lower;
		VS_FOREACH_IFACE(DO_CASE)
#undef DO_CASE
		default: return "unknown";
	}
}

int str_to_iface(const char * str, enum VS_IFACE * iface)
{
#define CHECK(__upper, __lower) \
	if (strcmp(str, #__upper) == 0 || strcmp(str, #__lower) == 0) { \
		if (iface) *iface = VS_##__upper; \
	} else

	VS_FOREACH_IFACE(CHECK)
		return 0;
#undef CHECK
	return 1;
}


#define DEF(__upper, __lower) static int __lower##_created_count = 0;
VS_FOREACH_IFACE(DEF)
#undef DEF

/*! Writes the name for a new device to the buffer pointed to by @buf.
 * Each call to this function creates a new name. */
void new_device_name(enum VS_IFACE iface, char * buf, size_t size)
{
	int * n;

	switch (iface) {
#define DO_CASE(__upper, __lower) case VS_##__upper: n = & __lower##_created_count; break;
		VS_FOREACH_IFACE(DO_CASE)
#undef DO_CASE
		default: return; /* can't happen? */
	}

	snprintf(buf, size, "%s%d", iface_to_str(iface), (*n)++);
}

