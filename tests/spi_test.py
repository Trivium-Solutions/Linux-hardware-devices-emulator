#!/usr/bin/env python3
import os
import sys

import config_sysfs as config

# https://github.com/romilly/quick2wire-python-api
from quick2wire.spi import *

# ----------------------------------------------------------------------

def throw(msg):
    raise Exception(msg)

# ----------------------------------------------------------------------

def is_module_loaded():
    return config.is_module_loaded(config.KMOD_NAME)

# ----------------------------------------------------------------------

def load_module():
    config.run(['insmod', '../kernel/' + config.KMOD_NAME + '.ko'])

# ----------------------------------------------------------------------

def unload_module():
    config.run(['rmmod', config.KMOD_NAME])

# ----------------------------------------------------------------------

def spi_check_query(bus, cs, request, expected_response):
    with SPIDevice(cs, bus) as spi:
        spi.transaction(writing(request))
        lst = spi.transaction(reading(len(expected_response)))
        print('  Written data: ', config.bytes_to_hex_str(request))
        print('  Expected data:', config.bytes_to_hex_str(expected_response))
        print('  Read data:    ', config.bytes_to_hex_str(lst[0]))
        return expected_response == lst[0]

# ----------------------------------------------------------------------

def sanity():
    req = b'\x01\x02\x03\x04'
    expected_resp = b'\xaa\xbb\xcc\xdd\xee\xff'

    pair = config.bytes_to_hex_str(req) + '=' + config.bytes_to_hex_str(expected_resp)

    sysfs_path = '/sys/kernel/' + config.KMOD_NAME + '/spi'

    # add new emulated device
    config.write_file(sysfs_path + '/add', '1')

    # add new request-response pair
    config.write_file(sysfs_path + '/spi0/add', pair)

    passed = spi_check_query(0, 0, req, expected_resp)

    print(passed and '  *** TEST PASSED ***' or '  *** TEST NOT PASSED ***')

    return passed

# ----------------------------------------------------------------------

def main():
    if os.geteuid() != 0:
        args = ['sudo', sys.executable] + sys.argv + [os.environ]
        os.execlpe('sudo', *args)

    if is_module_loaded():
        print(config.KMOD_NAME + ' is already loaded; will unload it first.')
        unload_module()

    if not config.is_spidev_loaded():
        print('spidev is not loaded; will load it first.')
        config.run(['modprobe', 'spidev'])

    load_module()

    try:
        sanity()

    finally:
        unload_module()

# ----------------------------------------------------------------------

if __name__ == '__main__':
    main()
