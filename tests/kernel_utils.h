/*!
   \file kernel_utils.h

   Declarations for some kernel code that we use in userspace
   for testing/debugging.
*/
#ifndef KERNEL_UTILS_H_INCLUDED
#define KERNEL_UTILS_H_INCLUDED 1

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

/* Kernel list implementation for userspace. */
#include "list.h"

/* Some compatibility macros */
#define likely
#define unlikely
#define simple_strtoul strtoul
#define jiffies_to_msecs
#define msecs_to_jiffies

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

struct kobj_attribute {
	char dummy;
};

extern int hex2bin(u8 *dst, const char *src, size_t count);
extern char *bin2hex(char *dst, const void *src, size_t count);
extern int scnprintf(char *buf, size_t size, const char *fmt, ...);
extern int vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

extern char *strnchr(const char *s, size_t count, int c);

#endif /* KERNEL_UTILS_H_INCLUDED */
