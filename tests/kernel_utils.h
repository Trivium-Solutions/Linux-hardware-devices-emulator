/*
   Declarations for some kernel code that we use in userspace
   for testing/debugging.
*/
#ifndef KERNEL_UTILS_H_INCLUDED
#define KERNEL_UTILS_H_INCLUDED 1

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

extern int hex2bin(u8 *dst, const char *src, size_t count);
extern char *bin2hex(char *dst, const void *src, size_t count);

extern char *strnchr(const char *s, size_t count, int c);

#endif /* KERNEL_UTILS_H_INCLUDED */
