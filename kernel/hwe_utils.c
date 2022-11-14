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
#	include <linux/jiffies.h>
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

/*! Parses a time representation in the format 1h2m3s4ms and returns time
 * in milliseconds. On error, 0 is returned.
 */
static inline unsigned hwe_str_to_time(const char * str, const char ** end_ptr)
{
	static const struct pattern_struct {
		const char * unit;
		int unit_len;
		unsigned max;
		unsigned mult;
	}
	pattern[] = {
#define P(__u, __max, __mult) \
	{ .unit = __u, .unit_len = sizeof(__u) - 1, .max = __max, .mult = __mult }
		/* XXX you can pass as many hours as you wish, but the
		 * return value will be checked for overflows */
		P("h",	INT_MAX,	60*60*1000),
		P("m",	59,		60*1000),
		P("s",	59,		1*1000),
		P("ms",	999,		1),
		P(0,	0,		0),
#undef P
	};
	const struct pattern_struct * p = 0;
	const char * s = str;
	unsigned long long ret = 0;
	unsigned long long n;
	char *ep;

	do {
		n = simple_strtoull(s, &ep, 10);

		if (s == ep) {
			/* error: no time value */
			ret = 0;
			break;
		}

		if (!p)
			p = pattern;

		for (; p->unit; p++)
			if (strncmp(p->unit, ep, p->unit_len) == 0 &&
			    !isalpha((unsigned char)ep[p->unit_len]))
				break;

		if (!p->unit || n > p->max) {
			/* error: invalid time unit/value */
			ret = 0;
			ep = (char *)s;
			break;
		}

		ret += n * p->mult;

		if (ret > UINT_MAX) {
			/* error: overflow */
			ret = 0;
			ep = (char *)str;
			break;
		}

		ep += p->unit_len;
		s = ep;
		p++;
#define IS_SEP(c) ((c) == 0 || (c) == ',' || (c) == '=')
	}
	while (p->unit && !IS_SEP(*s));

	if (!IS_SEP(*s))
		/* error: invalid separator */
		ret = 0;

	if (end_ptr)
		*end_ptr = ep;

	return ret;
}

/*! Returns a 1h2m3s4ms representation of time.
 */
static inline char * hwe_time_to_str(char * str, size_t size, unsigned t)
{
	int n = 0, h, m, s, ms;

	ms = t % 1000;
	t /= 1000;
	h = t / 3600;
	m = (t - 3600 * h) / 60;
	s = t - 3600 * h - m * 60;

	if (h)
		n = scnprintf(str, size, "%uh", h);

	if (m || (h && (s || ms)))
		n += scnprintf(str + n, size - n, "%um", m);

	if (s || ((h || m) && ms))
		n += scnprintf(str + n, size - n, "%us", s);

	if (ms)
		n += scnprintf(str + n, size - n, "%ums", ms);

	return str + n;
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

	if (is_hex_str(s, sz)) {
		if (sz > HWE_MAX_REQUEST * 2)
			return "request string too long";

		if (sz & 1)
			return "odd number of characters in request string";

		if (hex2bin(pair->req, s, sz / 2))
			return "invalid character in request string";

		pair->req_size = sz / 2;
		pair->async_rx = false;
	}
	else {
		unsigned t;
		const char * e;

		if (strncmp(s, "timer:", 6) == 0)
			s += 6;

		t = hwe_str_to_time(s, &e);

		if (!t || *e != '=')
			return "invalid data definition";

		pair->req_size = 0;
		pair->async_rx = true;
		pair->period_ms = t;
		pair->period = msecs_to_jiffies(t);
		pair->time = 0;
	}

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

	if (!pair->async_rx && (pair->req_size < 1 || pair->req_size > HWE_MAX_REQUEST))
		return "error: request size out of valid range";

	if (pair->resp_size < 1 || pair->resp_size > HWE_MAX_RESPONSE)
		return "error: response size out of valid range";

	if (pair->async_rx) {
		const size_t n = sizeof("timer:") - 1;

		strcpy(p, "timer:");
		p = hwe_time_to_str(p + n, sizeof(buf) - n, pair->period_ms);
	}
	else
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
		/* XXX skip the pairs used in asynchronous data exchange */
		if (!ret->async_rx && ret->req_size == req_size &&
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
