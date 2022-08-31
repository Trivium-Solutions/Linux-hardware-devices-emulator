/*!
 * \file hwe_consts.h
 * \brief Global constants
 *
 */
#ifndef HWE_CONSTS_H_INCLUDED
#define HWE_CONSTS_H_INCLUDED 1

/*! Maximum length of a request, given the maximum sysfs file size */
#define	HWE_MAX_REQUEST	((4096 - 1) / 4)

/*! Maximum length of a response, given the maximum sysfs file size */
#define	HWE_MAX_RESPONSE	((4096 - 1) / 4)

/*! Maximum length of a request-response string */
#define HWE_MAX_PAIR_STR	(HWE_MAX_REQUEST * 2 + HWE_MAX_RESPONSE * 2 + 1)

/*! Maximum number of key-value pairs that can be added to a device */
#define	HWE_MAX_PAIRS	1000

/*! Maximum number of devices per interface
 *
 * This limitation is posed by some infrastructures, e.g.
 * SPI: https://elixir.bootlin.com/linux/v5.19/source/drivers/spi/spidev.c#L44
 */
#define	HWE_MAX_DEVICES	256

/*! Currently supported device types.
    Start adding new interfaces from here. */
#define HWE_FOREACH_IFACE(D) \
	D(TTY, tty) \
	D(I2C, i2c) \
	D(NET, net) \
	D(SPI, spi) \


#define DEFINE_IFACE(__upper, __lower) HWE_##__upper,
/*! Internal identifiers for device types. */
enum HWE_IFACE {
	HWE_FOREACH_IFACE(DEFINE_IFACE)
	HWE_IFACE_COUNT
};
#undef DEFINE_IFACE

#endif /* HWE_CONSTS_H_INCLUDED */
