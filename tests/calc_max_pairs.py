#!/usr/bin/env python3
'''
    This script calculates the maximum number of request-response pairs
    for a single device. We have to restrict this number so that all
    the pairs can fit in a sysfs file which cannot exceed PAGE_SIZE
    bytes in length (4096 bytes on i386).

'''

# Maximum length of a request
_VS_MAX_REQUEST = 64
#_VS_MAX_REQUEST = 8

# Maximum length of a response
_VS_MAX_RESPONSE = 64
#_VS_MAX_RESPONSE = 8

_PAGE_SIZE = 4096

max_pairs = 0
max_size = 0

while True:
    # <line_no> '\t' <request> '=' <response> '\n'
    # each byte in request-response take 2 bytes as a hexadecimal numer
    sz = len(str(max_pairs)) + 1 + _VS_MAX_REQUEST * 2 + 1 + _VS_MAX_RESPONSE * 2 + 1
    if max_size + sz >= _PAGE_SIZE:
        break
    max_size += sz
    max_pairs += 1


print('#define\tVS_MAX_PAIRS\t%d' % max_pairs)
print('/* Max data size: %d */' % max_size)

