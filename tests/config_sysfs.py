#!/usr/bin/env python3
'''
    Configuring and testing routines

    This module was started as a collection of testing routines,
    but now it is also used by the control utility.

    TODO split this into modules containing sysfs, configure and system
         functions respectively (?)
'''
import os
import sys
import random
import subprocess

IF_I2C = 'i2c'
IF_TTY = 'tty'

IFACES = (IF_I2C, IF_TTY)

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

def is_hex_str(s):
    for c in s:
        if c not in '0123456789abcdefABCDEF':
            return False
    return True

# ----------------------------------------------------------------------

def bytes_to_hex_str(b):
    return ''.join('%02x' % x for x in b)

# ----------------------------------------------------------------------

def rand_hex_str(max):
    return bytes_to_hex_str(bytes(random.randint(0, 255) for i in rand_range(1, max)))

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

_TTY_DEV_SYMLINKS = True
#_TTY_DEV_SYMLINKS = False

# XXX name of the tty device VcpSdkCmd uses
VcpSdkCmd_TTY_NAME = '/dev/ttyUSB'

VCPSIM_TTY_NAME = '/dev/ttyVCP'

def ifaces_init(config):

    def print_avail(d1, d2):
        print(d1, 'as', d2)

    # i2c

    ifc = IF_I2C

    # We cannot access i2c devices from userspace without the i2c-dev
    # driver loaded.

    if not is_module_loaded('i2c_dev'):
        run(['modprobe', 'i2c_dev'])

    i2c_dev_dir = '/sys/class/i2c-dev'
    sstr = 'adapter '
    devs = {}

    print('Available devices:')
    print('------------------')

    # Collect our /dev/i2c* device names from /sys/class/i2c-dev

    with os.scandir(i2c_dev_dir) as it:
        for e in it:
            if e.is_dir():
                fstr = read_file('/'.join([i2c_dev_dir, e.name, 'name']))
                if KMOD_NAME in fstr:
                    n = fstr.find(sstr)
                    if n >= 0:
                        devs[ifc + fstr[n + len(sstr)]] = '/dev/' + e.name

    for d in sorted(devs, key = lambda d: int(d[len(ifc):])):
        print_avail(d, devs[d])

    # tty

    import glob

    ifc = IF_TTY

    n = 0
    for df in sorted(glob.glob(VCPSIM_TTY_NAME + '*'), key = lambda d: int(d[len(VCPSIM_TTY_NAME):])):
        dev_num = df[len(VCPSIM_TTY_NAME):]
        if _TTY_DEV_SYMLINKS:
            while True:
                lnk = VcpSdkCmd_TTY_NAME + str(n)
                if not os.path.exists(lnk):
                    break
                n += 1
            os.symlink(df, lnk)
            print_avail(ifc + dev_num, lnk)
        else:
            print_avail(ifc + dev_num, df)

# ----------------------------------------------------------------------

def ifaces_cleanup():

    # i2c

    # Earlier on we ensured that i2c-dev was loaded and now we have
    # no way of knowing whether or not it had been loaded before. So we
    # just leave it loaded :(

    # tty

    if _TTY_DEV_SYMLINKS:
        import glob

        # remove all symlinks linking to our devices
        for lnk in glob.glob(VcpSdkCmd_TTY_NAME + '*'):
            if os.path.islink(lnk):
                fn = os.path.realpath(lnk)
                if VCPSIM_TTY_NAME in fn:
                    os.remove(lnk)

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
    '''
        Configuration test

        1. create a random configuration;
        2. load it into the module;
        3. read the configuration from the module;
        4. compare what was written with what was read;
        5. repeat the previous steps.
    '''

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

