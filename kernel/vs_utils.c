#ifdef __KERNEL__
#	include <linux/kernel.h>
#	include <linux/list.h>
#	include <linux/string.h>
#	include <linux/ctype.h>
#else
#	include <kernel_utils.h>
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

/*! Key-value string parser
 */
const char * str_to_pair(const char * str, size_t str_size, struct vs_pair * pair)
{
	const char * s = str;
	char * e;
	int sz;

	if (!str_size)
		return "empty string";

	e = strnchr(s, str_size, '=');

	if (!e)
		return "missing '='";

	sz = e - s;

	if (!sz)
		return "empty request";

	if (sz > VS_MAX_REQUEST * 2)
		return "request string too long";

	if (sz & 1)
		return "odd number of characters in request string";

	if (hex2bin(pair->req, s, sz / 2))
		return "invalid character in request string";

	pair->req_size = sz / 2;

	sz++; e++; /* '=' */

	sz = str_size - sz;
	s = e;

	/* may have a terminating newline character */
	e = strnchr(s, sz, '\n');

	if (e)
		sz = e - s;

	if (!sz)
		return "empty response";

	if (sz > VS_MAX_RESPONSE * 2)
		return "response string too long";

	if (sz & 1)
		return "odd number of characters in response string";

	if (hex2bin(pair->resp, s, sz / 2))
		return "invalid character in response string";

	pair->resp_size = sz / 2;

	return NULL;
}

/*! Key-value string maker
 */
const char * pair_to_str(struct vs_pair * pair)
{
	/* each hex digit takes 2 chars + '=' + terminating null */
	static char buf[VS_MAX_REQUEST * 2 + VS_MAX_RESPONSE * 2 + 1 + 1];
	char * p = buf;

	if (pair->req_size < 1 || pair->req_size > VS_MAX_REQUEST)
		return "error: request size out of valid range";

	if (pair->resp_size < 1 || pair->resp_size > VS_MAX_RESPONSE)
		return "error: response size out of valid range";

	p = bin2hex(p, pair->req, pair->req_size);

	*p++ = '=';

	p = bin2hex(p, pair->resp, pair->resp_size);

	*p = 0;

	return buf;
}

struct vs_pair * find_pair(struct list_head * list, const unsigned char * request, size_t req_size)
{
	struct vs_pair * ret;

	list_for_each_entry (ret, list, entry) {
		if (ret->req_size == req_size &&
		    memcmp(ret->req, request, req_size) == 0)
			return ret;
	}

	return NULL;
}

struct vs_pair * get_pair_at_index(struct list_head * list, size_t index)
{
	size_t count = 0;
	struct vs_pair * ret;

	list_for_each_entry (ret, list, entry) {
		if (index == count)
			return ret;
		count++;
	}

	return NULL;
}
