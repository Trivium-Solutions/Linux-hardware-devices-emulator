#!/usr/bin/env python3
import os
import sys
import threading
import time

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

def green(text):
    return '\033[32m' + text + '\033[0m'

# ----------------------------------------------------------------------

def red(text):
    return '\033[31m' + text + '\033[0m'

# ----------------------------------------------------------------------

def cyan(text):
    return '\033[36m' + text + '\033[0m'

# ----------------------------------------------------------------------

def yellow(text):
    return '\033[33m' + text + '\033[0m'

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
        print(IND2 + (ok and green('*** PASSED ***') or
            red('*** FAILED ***')))

        return ok

# ----------------------------------------------------------------------

def zero_bytes(b):
    for x in b:
        if x != 0:
            return False
    return True

# ----------------------------------------------------------------------

max_data_len = 0
wait_time = 0

class ReaderThread(threading.Thread):
    def __init__(self, dev_name, bus, cs, data):
        threading.Thread.__init__(self)
        self.dev_name = dev_name
        self.bus = bus
        self.cs = cs
        self.data = data

    def run(self):
        with SPIDevice(self.cs, self.bus) as spi:
            t = time.time()
            end_time = t + wait_time
            while t < end_time:
                lst = spi.transaction(reading(max_data_len))
                data = len(lst) > 0 and lst[0] or None
                if not data is None and not zero_bytes(data):
                    ns = time.time_ns()
                    ms = ns / 1000000
                    pfx = time.strftime('[%H:%M:%S.', time.localtime(ms / 1000))
                    pfx += ('%03d] %s:' % (int(ms) % 1000, cyan(self.dev_name)))
                    print(pfx, ' '.join('%02x' % x for x in data))
                t = time.time()

# ----------------------------------------------------------------------

def main():
    argc = len(sys.argv)
    argv = sys.argv
    if argc != 2 and argc != 4:
        print('Usage:')
        print('    FIRST:')
        print('    $ hwectl start <ini-filename>')
        print('    THEN:')
        print('    $ sudo python3 %s <ini-filename>' %
            (os.path.basename(__file__)))
        print('    OR:')
        print('    $ sudo python3 %s --async <time_in_sec> <ini-filename>' %
            (os.path.basename(__file__)))
        return 1

    exitcode = 0

    is_async = argc == 4

    filename = is_async and argv[3] or argv[1]

    global wait_time

    if is_async:
        wait_time = int(argv[2])

    cfg = hwectl.load_from_ini(filename)

    def error(msg):
        throw(('%s.\nMake sure the file "%s" has been loaded by using hwectl') %
              (msg, filename))

    def on_pair(iface_name, dev_name, pair_num, pair):
        if iface_name != config.IF_SPI:
            return

        if config.is_async_pair(pair):
            # not supporting asynchronous data here
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

        print(cyan('%s:' % d))

        ok = spi_check_query(int(nums[0]), int(nums[1]), bytes.fromhex(p[0]), bytes.fromhex(p[1]))

        nonlocal exitcode

        if not ok:
            exitcode = 1

    async_data = {}

    def on_pair_async(iface_name, dev_name, pair_num, pair):
        if iface_name != config.IF_SPI:
            return

        if not config.is_async_pair(pair):
            # supporting only asynchronous data here
            return

        dev = cfg[iface_name][dev_name].get('_extern_dev_name')
        lnk = '/dev/' + dev

        if not os.path.islink(lnk):
            error('File not found: ' + lnk)

        tgt = os.path.realpath(lnk)

        if not tgt.startswith('/dev/spidev'):
            error('Wrong link: %s -> %s' % (lnk, tgt))

        nums = tgt[len('/dev/spidev'):].split('.')

        if len(nums) != 2 or not nums[0].isdigit() or not nums[1].isdigit():
            error('Wrong link: %s -> %s' % (lnk, tgt))

        p = pair.split('=')
        d = bytes.fromhex(p[1])

        global max_data_len

        if len(d) > max_data_len:
            max_data_len = len(d)

        if not lnk in async_data:
            async_data[lnk] = { 'target': tgt, 'bus': int(nums[0]), 'cs': int(nums[1]), 'data': [] }

        async_data[lnk]['data'].append(d)

    on_pair_fn = is_async and on_pair_async or on_pair

    config.traverse_config(cfg, on_iface = None, on_dev = None, on_pair = on_pair_fn)

    if not is_async:
        return exitcode

    # here goes async testing

    threads = []

    for dev, e in async_data.items():
        thrd = ReaderThread(dev, e['bus'], e['cs'], e['data'])
        threads.append(thrd)
        thrd.start()

    for thrd in threads:
        thrd.join()

    return exitcode

# ----------------------------------------------------------------------

if __name__ == '__main__':
    sys.exit(main())
