/*!
 * \file hwe_utils.c
 * \brief Utility functions
 *
 */
#ifdef __KERNEL__
#	include <linux/kobject.h>
#	include <linux/kernel.h>
#	include <linux/list.h>
#	include <linux/string.h>
#	include <linux/ctype.h>
#else
#	include <kernel_utils.h>
#endif

#include "hwemu.h"

/*! Returns a string with the name of iface. */
const char * iface_to_str(enum HWE_IFACE iface)
{
	switch (iface) {
#define DO_CASE(__upper, __lower) case HWE_##__upper: return #__lower;
		HWE_FOREACH_IFACE(DO_CASE)
#undef DO_CASE
		default: return "unknown";
	}
}

/*! Returns non-zero if \a str is the name of an interface, in which case
    the variable pointed to by \a iface is assigned the interface number. */
int str_to_iface(const char * str, enum HWE_IFACE * iface)
{
#define CHECK(__upper, __lower) \
	if (strcmp(str, #__upper) == 0 || strcmp(str, #__lower) == 0) { \
		if (iface) *iface = HWE_##__upper; \
	} else

	HWE_FOREACH_IFACE(CHECK)
		return 0;
#undef CHECK
	return 1;
}

/*! Returns non-zero if \a str is a string of hexadecimal numeric characters. */
static inline int is_hex_str(const char * str, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (!isxdigit((unsigned char)str[i]))
			return 0;

	return 1;
}

/*! Parses a time representation in the format 1h2m3s and returns time
 * in seconds. On error, 0 is returned.
 */
static inline unsigned hwe_str_to_time(const char * str, const char ** end_ptr)
{
	static const struct pattern_struct {
		char ch;
		unsigned long max;
		unsigned long mult;
	}
	pattern[] = {
		{ .ch = 'h', .max = ULONG_MAX, .mult = 60*60 },
		{ .ch = 'm', .max = 59, .mult = 60 },
		{ .ch = 's', .max = 59, .mult = 1 },
		{ .ch = 0, },
	};
	const struct pattern_struct * p = 0;
	const char * s = str;
	unsigned long ret = 0;
	char *ep;
	unsigned long n;

	do {
		n = simple_strtoul(s, &ep, 10);
		if (p) {
			if (p->ch != *ep) {
				/* error: invalid time unit */
				ret = 0;
				break;
			}
		}
		else {
			for (p = pattern; p->ch; p++)
				if (p->ch == *ep)
					break;
			if (!p->ch)
				/* error: invalid time unit */
				break;
		}

		if (n > p->max) {
			/* error: invalid time value */
			ret = 0;
			ep = (char *)s;
			break;
		}

		s = ++ep;
		ret += n * p->mult;
		p++;
	}
	while (p->ch && *s);

	if (*s != 0 && *s != ',' && *s != '=')
		/* error: invalid separator */
		ret = 0;

	if (end_ptr)
		*end_ptr = ep;

	return ret;
}

/*! Returns a HMS representation of time.
 */
static inline const char * hwe_time_to_str(unsigned t)
{
	int h, m, s;
	static char ret[32];

	h = t / 3600;
	m = (t - 3600 * h) / 60;
	s = t - 3600 * h - m * 60;

	if (h)
		snprintf(ret, sizeof(ret), "%uh%dm%ds", h, m, s);
	else
	if (m)
		snprintf(ret, sizeof(ret), "%dm%ds", m, s);
	else
		snprintf(ret, sizeof(ret), "%ds", s);

	return ret;
}

/*! Key-value string parser
 */
const char * str_to_pair(const char * str, size_t str_size, struct hwe_pair * pair)
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

	if (sz > HWE_MAX_REQUEST * 2)
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

	if (sz > HWE_MAX_RESPONSE * 2)
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
const char * pair_to_str(struct hwe_pair * pair)
{
	static char buf[HWE_MAX_PAIR_STR + 1];
	char * p = buf;

	if (pair->req_size < 1 || pair->req_size > HWE_MAX_REQUEST)
		return "error: request size out of valid range";

	if (pair->resp_size < 1 || pair->resp_size > HWE_MAX_RESPONSE)
		return "error: response size out of valid range";

	p = bin2hex(p, pair->req, pair->req_size);

	*p++ = '=';

	p = bin2hex(p, pair->resp, pair->resp_size);

	*p = 0;

	return buf;
}

struct hwe_pair * find_pair(struct list_head * list, const unsigned char * request, size_t req_size)
{
	struct hwe_pair * ret;

	list_for_each_entry (ret, list, entry) {
		if (ret->req_size == req_size &&
		    memcmp(ret->req, request, req_size) == 0)
			return ret;
	}

	return NULL;
}

struct hwe_pair * get_pair_at_index(struct list_head * list, size_t index)
{
	struct hwe_pair * ret;

	list_for_each_entry (ret, list, entry) {
		if (index == ret->index)
			return ret;
	}

	return NULL;
}
