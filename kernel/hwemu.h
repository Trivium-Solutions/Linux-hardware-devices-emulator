#ifndef HWEMU_H_INCLUDED
#define HWEMU_H_INCLUDED 1

#define DRIVER_NAME     "hwemu"

#ifdef pr_fmt
#undef pr_fmt
#endif

/*! Log message format. */
#define pr_fmt(fmt) DRIVER_NAME ": " fmt

#define COUNTOF(a)      (sizeof(a)/sizeof((a)[0]))
#define FOR_EACH(entry, list) for (entry = list; entry < list + COUNTOF(list); (entry)++)

/*! Currently supported device types.
    Start adding new interfaces from here. */
#define HWE_FOREACH_IFACE(D) \
	D(TTY, tty) \
	D(I2C, i2c) \


#define DEFINE_IFACE(__upper, __lower) HWE_##__upper,
/*! Internal identifiers for device types. */
enum HWE_IFACE {
	HWE_FOREACH_IFACE(DEFINE_IFACE)
	HWE_IFACE_COUNT
};
#undef DEFINE_IFACE

/*! Maximum length of a request, given the maximum sysfs file size */
#define	HWE_MAX_REQUEST	((4096 - 1) / 4)

/*! Maximum length of a response, given the maximum sysfs file size */
#define	HWE_MAX_RESPONSE	((4096 - 1) / 4)

/*! Maximum number of key-value pairs that can be added to a device */
#define	HWE_MAX_PAIRS	1000

/*! Maximum number of devices per interface */
#define	HWE_MAX_DEVICES	256

/*! Generic device. */
struct hwe_dev;

/*! Device implementation for particular interfaces. */
struct hwe_dev_priv;

#define HWE_STR(x) #x
#define HWE_STRLEN(x) (sizeof(HWE_STR(x)) - 1)

/*! Request-response pair */
struct hwe_pair {
	struct list_head entry;
	unsigned char req[HWE_MAX_REQUEST];
	size_t req_size;
	unsigned char resp[HWE_MAX_RESPONSE];
	size_t resp_size;
	struct hwe_dev * dev;
	long index;
	/* filename is the string value of a decimal integer
	 * in range 0..HWE_MAX_PAIRS */
	char filename[HWE_STRLEN(HWE_MAX_PAIRS) + 1];
	struct kobj_attribute pair_file;
};

/*! Returns the number of entries in a list */
static inline size_t list_entry_count(struct list_head * list)
{
	size_t ret = 0;
	struct list_head * p;

	list_for_each (p, list)
		ret++;

	return ret;
}

/* in hwe_utils.c */
const char * iface_to_str(enum HWE_IFACE iface);
int str_to_iface(const char * str, enum HWE_IFACE * iface);
const char * str_to_pair(const char * str, size_t str_size, struct hwe_pair * pair);
const char * pair_to_str(struct hwe_pair * pair);
struct hwe_pair * find_pair(struct list_head * list, const unsigned char * request, size_t req_size);
struct hwe_pair * get_pair_at_index(struct list_head * list, size_t index);

/* in hwe_sysfs.c */
struct hwe_pair * find_response(struct hwe_dev * dev,
	const unsigned char * request, int req_size);
void lock_devs(struct hwe_dev * dev);
void unlock_devs(struct hwe_dev * dev);
void lock_iface_devs(enum HWE_IFACE iface);
void unlock_iface_devs(enum HWE_IFACE iface);

/* in hwe_main.c */
void hwe_log_request(enum HWE_IFACE iface, long dev_num,
	const void * request, size_t req_size, bool have_response);
void hwe_log_response(enum HWE_IFACE iface, long dev_num,
	const void * response, size_t resp_size);

#endif /* HWEMU_H_INCLUDED */
