#!/usr/bin/env python3
import os
import sys

PROG_DIR = os.path.dirname(os.path.realpath(__file__))

_IMPORT_DIRS = ('../control',)

for d in _IMPORT_DIRS:
    p = os.path.realpath('/'.join((PROG_DIR, d)))
    if os.path.isdir(p):
        sys.path.append(p)

import hwectl
import config_sysfs as config

# https://github.com/romilly/quick2wire-python-api
from quick2wire.spi import *

# ----------------------------------------------------------------------

def throw(msg):
    raise Exception(msg)

# ----------------------------------------------------------------------

def spi_check_query(bus, cs, request, expected_response):
    IND1 = ' ' * 8
    IND2 = ' ' * 16

    def hexlns(data):
        s = config.bytes_to_hex_str(data)
        ret = ''
        W = 64
        for i in range(0, len(s), W):
            ret += IND2 + s[i: i + W]
            if i + W < len(s):
                ret += '\n'
        return ret

    with SPIDevice(cs, bus) as spi:
        spi.transaction(writing(request))
        lst = spi.transaction(reading(len(expected_response)))
        ok = expected_response == lst[0]

        print(IND1 + 'Written data:')
        print(hexlns(request))
        print(IND1 + 'Expected data:')
        print(hexlns(expected_response))
        print(IND1 + 'Read data:')
        print(hexlns(lst[0]))
        print(IND1 + 'Result:')
        print(IND2 + (ok and '\033[32m*** PASSED ***\033[0m' or
            '\033[31m*** FAILED ***\033[0m'))

        return ok

# ----------------------------------------------------------------------

def main():
    if len(sys.argv) != 2:
        print('Usage:')
        print('    $ hwectl start <ini-filename>')
        print('    $ sudo python3 %s <ini-filename>' %
            (os.path.basename(__file__)))
        return 1

    exitcode = 0

    filename = sys.argv[1]

    cfg = hwectl.load_from_ini(filename)

    def error(msg):
        throw(('%s.\nMake sure the file "%s" has been loaded by using hwectl') %
              (msg, filename))

    def on_pair(iface_name, dev_name, pair_num, pair):
        if iface_name != config.IF_SPI:
            return

        d = cfg[iface_name][dev_name].get('_extern_dev_name')
        lnk = '/dev/' + d

        if not os.path.islink(lnk):
            error('File not found: ' + lnk)

        tgt = os.path.realpath(lnk)

        if not tgt.startswith('/dev/spidev'):
            error('Wrong link: %s -> %s' % (lnk, tgt))

        nums = tgt[len('/dev/spidev'):].split('.')

        if len(nums) != 2 or not nums[0].isdigit() or not nums[1].isdigit():
            error('Wrong link: %s -> %s' % (lnk, tgt))

        p = pair.split('=')

        print('\033[36m%s\033[0m:' % d)

        ok = spi_check_query(int(nums[0]), int(nums[1]), bytes.fromhex(p[0]), bytes.fromhex(p[1]))

        nonlocal exitcode

        if not ok:
            exitcode = 1

    config.traverse_config(cfg, on_iface = None, on_dev = None, on_pair = on_pair)

    return exitcode

# ----------------------------------------------------------------------

if __name__ == '__main__':
    sys.exit(main())
