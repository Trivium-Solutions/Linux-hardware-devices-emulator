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

#define	VS_MAX_REQUEST	128
#define	VS_MAX_RESPONSE	128

struct vs_pair {
	char req[VS_MAX_REQUEST];
	size_t req_size;
	char resp[VS_MAX_RESPONSE];
	size_t resp_size;
};

const char * iface_to_str(enum VS_IFACE iface);
int str_to_iface(const char * str, enum VS_IFACE * iface);
void new_device_name(enum VS_IFACE iface, char * buf, size_t size);
const char * str_to_pair(const char * str, size_t str_size, struct vs_pair * pair);
const char * pair_to_str(struct vs_pair * pair);

#endif /* VCPSIM_H_INCLUDED */

