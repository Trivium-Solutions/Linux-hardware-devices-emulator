#ifndef VCPSIM_H_INCLUDED
#define VCPSIM_H_INCLUDED 1

#define DRIVER_NAME     "vcpsim"

#ifdef pr_fmt
#undef pr_fmt
#endif

/*! Log message format. */
#define pr_fmt(fmt) DRIVER_NAME ": " fmt

#define COUNTOF(a)      (sizeof(a)/sizeof((a)[0]))
#define FOR_EACH(entry, list) for (entry = list; entry < list + COUNTOF(list); (entry)++)

/*! Currently supported device types.
    Start adding new interfaces from here. */
#define VS_FOREACH_IFACE(D) \
	D(TTY, tty) \
	D(I2C, i2c) \


#define DEFINE_IFACE(__upper, __lower) VS_##__upper,
/*! Internal identifiers for device types. */
enum VS_IFACE {
	VS_FOREACH_IFACE(DEFINE_IFACE)
	VS_IFACE_COUNT
};
#undef DEFINE_IFACE

/*! Maximum length of a request
 * \warning If you change this value, you must recalculate VS_MAX_PAIRS. */
#define	VS_MAX_REQUEST	64
//#define	VS_MAX_REQUEST	8

/*! Maximum length of a response
 * \warning If you change this value, you must recalculate VS_MAX_PAIRS. */
#define	VS_MAX_RESPONSE	64
//#define	VS_MAX_RESPONSE	8

/*! Maximum number of key-value pairs that can be added to a device
 * \warning This value depends on VS_MAX_REQUEST and VS_MAX_RESPONSE. If you
 * change either of them, you must also recalculate VS_MAX_PAIRS!
 * This value ensures that all the pairs can fit in a sysfs file, which
 * cannot exceed PAGE_SIZE in length. */
#define	VS_MAX_PAIRS	15
//#define	VS_MAX_PAIRS	110

/*! Maximum number of devices per interface */
#define	VS_MAX_DEVICES	8


/*! Request-response pair */
struct vs_pair {
	struct list_head entry;
	unsigned char req[VS_MAX_REQUEST];
	size_t req_size;
	unsigned char resp[VS_MAX_RESPONSE];
	size_t resp_size;
};

/*! Generic device. */
struct vs_dev;

/*! Device implementation for particular interfaces. */
struct vs_dev_priv;

/*! Returns the number of entries in a list */
static inline size_t list_entry_count(struct list_head * list)
{
	size_t ret = 0;
	struct list_head * p;

	list_for_each (p, list)
		ret++;

	return ret;
}

/* in vs_utils.c */
const char * iface_to_str(enum VS_IFACE iface);
int str_to_iface(const char * str, enum VS_IFACE * iface);
const char * str_to_pair(const char * str, size_t str_size, struct vs_pair * pair);
const char * pair_to_str(struct vs_pair * pair);
struct vs_pair * find_pair(struct list_head * list, const unsigned char * request, size_t req_size);
struct vs_pair * get_pair_at_index(struct list_head * list, size_t index);

/* in vs_sysfs.c */
struct vs_pair * find_response(struct vs_dev * dev,
	const unsigned char * request, int req_size);

/* in vs_main.c */
void vs_log_request(enum VS_IFACE iface, long dev_num,
	const void * request, size_t req_size, bool have_response);
void vs_log_response(enum VS_IFACE iface, long dev_num,
	const void * response, size_t resp_size);

#endif /* VCPSIM_H_INCLUDED */

