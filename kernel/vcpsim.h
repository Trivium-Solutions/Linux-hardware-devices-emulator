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

#endif /* VCPSIM_H_INCLUDED */

