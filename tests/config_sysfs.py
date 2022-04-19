#!/usr/bin/env python3
'''
Testing routines
'''
import os
import sys
import random
import subprocess

IFACES = ('i2c', 'tty')

KMOD_NAME = 'vcpsim'

SYSFS_BASE_DIR = '/sys/kernel/' + KMOD_NAME

# Maximum length of a request
_VS_MAX_REQUEST = 64

# Maximum length of a response
_VS_MAX_RESPONSE = 64

# Minimum number of key-value pairs that can be added to a device
_VS_MIN_PAIRS = 0

# Maximum number of key-value pairs that can be added to a device
_VS_MAX_PAIRS = 1000


# Maximum number of devices per interface
_VS_MAX_DEVICES = 8

# ----------------------------------------------------------------------

def rand_range(min, max):
    return range(random.randint(min, max))

# ----------------------------------------------------------------------

def rand_hex_str(max):
    return ''.join('%02x' % x for x in bytearray(random.randint(0, 255) \
        for i in rand_range(1, max)))

# ----------------------------------------------------------------------

_used_requests = []

def rand_pair_str():
    while True:
        # avoid pair duplicates
        req = rand_hex_str(_VS_MAX_REQUEST)
        if not req in _used_requests:
            _used_requests.append(req)
            return req + '=' + rand_hex_str(_VS_MAX_RESPONSE)

# ----------------------------------------------------------------------

def rand_pair_str_list():
    return { i: rand_pair_str() for i in rand_range(_VS_MIN_PAIRS, _VS_MAX_PAIRS - 1) }

# ----------------------------------------------------------------------

def rand_dev_list(pfx):
    return { '%s%d' % (pfx, i) : rand_pair_str_list() for i in \
        rand_range(1, _VS_MAX_DEVICES) }

# ----------------------------------------------------------------------

def rand_config():
    global _used_requests
    _used_requests = []
    return { ifc: rand_dev_list(ifc) for ifc in IFACES }

# ----------------------------------------------------------------------

def traverse_config(config, *, on_iface, on_dev, on_pair):

    if on_iface is None:
        on_iface = lambda iface_name: None

    if on_dev is None:
        on_dev = lambda iface_name, dev_name: None

    if on_pair is None:
        on_pair = lambda iface_name, dev_name, pair_num, pair: None

    for iface_name, devs in config.items():

        ret = on_iface(iface_name)
        if ret is not None:
            return ret

        # sort device names by appended numbers, e.g. tty0, tty1, ... tty9, tty10, ...
        dev_names = sorted(devs, key = lambda d: int(d[len(iface_name):]))

        for dev_name in dev_names:
            if dev_name.startswith('_'):
                continue

            ret = on_dev(iface_name, dev_name)
            if ret is not None:
                return ret

            dev = devs[dev_name]

            # sort pairs by numbers
            for pair_num in sorted(dev):
                ret = on_pair(iface_name, dev_name, pair_num, dev[pair_num])
                if ret is not None:
                    return ret

    # return None

# ----------------------------------------------------------------------

def config_to_str(config):
    ret = ''

    def on_iface(iface_name):
        nonlocal ret
        ret += '%s\n' % (iface_name)

    def on_dev(iface_name, dev_name):
        nonlocal ret
        ret += '%s/%s\n' % (iface_name, dev_name)

    def on_pair(iface_name, dev_name, pair_num, pair):
        nonlocal ret
        ret += '%d\t%s\n' % (pair_num, pair)

    traverse_config(config, on_iface = on_iface, on_dev = on_dev, on_pair = on_pair)

    return ret

# ----------------------------------------------------------------------

def config_to_pretty_str(config):
    ret = ''

    def on_iface(iface_name):
        nonlocal ret
        ret += '  %s:\n' % (iface_name)

    def on_dev(iface_name, dev_name):
        nonlocal ret
        ret += '    %s:\n' % (dev_name)
        ret += '      %d pair(s)\n' % (len(config[iface_name][dev_name]))

    traverse_config(config, on_iface = on_iface, on_dev = on_dev, on_pair = None)

    return ret

# ----------------------------------------------------------------------

def read_file(filename):
    with open(filename, 'rt') as f:
        return f.read()

# ----------------------------------------------------------------------

def write_file(filename, data):
    with open(filename, 'wt') as f:
        sz = f.write(data)
        if sz != len(data):
            raise Exception('written only %d byte(s) of %d to file %s' %
                (sz, len(data), filename))

# ----------------------------------------------------------------------

def get_dir_names(path):
    return [e.name for e in os.scandir(path) if e.is_dir()]

# ----------------------------------------------------------------------

def get_file_names(path):
    return [e.name for e in os.scandir(path) if e.is_file()]

# ----------------------------------------------------------------------

def write_config(config):
    '''
    Write config to sysfs

    Assume:
        1. arrays of devices and pairs have no gaps;
        2. no config is currently loaded in sysfs.

    '''
    path = SYSFS_BASE_DIR

    def on_dev(iface_name, dev_name):
        nonlocal path
        f = '%s/%s/add' % (path, iface_name)
        write_file(f, '1')

    def on_pair(iface_name, dev_name, pair_num, pair):
        nonlocal path
        f = '%s/%s/%s/add' % (path, iface_name, dev_name)
        write_file(f, pair)

    return traverse_config(config, on_iface = None, on_dev = on_dev,\
        on_pair = on_pair) is None

# ----------------------------------------------------------------------

def read_pairs(dirname):
    ret = {}

    for fname in get_file_names(dirname):
        with open(dirname + '/' + fname, 'rt') as f:
            ret[int(fname)] = f.read()
    return ret

# ----------------------------------------------------------------------

def read_config():
    ret = {}

    for ifc in IFACES:

        iface_path = '%s/%s' % (SYSFS_BASE_DIR, ifc)

        ret[ifc] = {}

        for dev_name in get_dir_names(iface_path):
            lst = read_pairs('%s/%s/pairs' % (iface_path, dev_name))

            ret[ifc][dev_name] = lst

    return ret

# ----------------------------------------------------------------------

def erase_config():

    for ifc in IFACES:
        iface_path = '%s/%s' % (SYSFS_BASE_DIR, ifc)

        for dev_name in get_dir_names(iface_path):
            write_file(iface_path + '/uninstall', dev_name)

# ----------------------------------------------------------------------

def compare_config(cfg1, cfg2):
    return config_to_str(cfg1) == config_to_str(cfg2)

# ----------------------------------------------------------------------

def is_module_loaded(module):
    with open('/proc/modules', 'rt') as f:
        for line in f.readlines():
            if module in line:
                return True
    return False

# ----------------------------------------------------------------------

def run(args):
    p = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    p.wait()
    if p.returncode == 0:
        return
    err = p.stdout.read().decode().rstrip()
    if err == '':
        err = 'Command terminated with exit code %d: %s' % (p.returncode, ' '.join(args))
    raise Exception(err)

# ----------------------------------------------------------------------

def test_random_config_write():

    if os.geteuid() != 0:
        sys.exit('You must be root to run this script')

    if is_module_loaded('vcpsim'):
        # The kernel module is already loaded. We will reload it
        # to ensure the purity of the experiment.
        run(['rmmod', KMOD_NAME])

    run(['insmod', '../kernel/' + KMOD_NAME + '.ko'])

    random.seed()

    _REPEATS = 50

    for i in range(_REPEATS):
        print('--- Test %d of %d --------------------' % (i + 1, _REPEATS))
        print('Creating random config...')

        cfg1 = rand_config()

        print('Config:')
        print(config_to_pretty_str(cfg1), end = '')
        print('Writing config...')

        write_config(cfg1)

        print('Reading config...')

        cfg2 = read_config()

        erase_config()

        s1 = config_to_str(cfg1)
        s2 = config_to_str(cfg2)

        print('Comparing configs...')

        if s1 == s2:
            print('OK')
        else:
            print('*** ERROR: Configs do not match!')
            print(s1)
            print('--------------------------------')
            print(s2)

    run(['rmmod', KMOD_NAME])


# ----------------------------------------------------------------------

if __name__ == '__main__':
    test_random_config_write()

