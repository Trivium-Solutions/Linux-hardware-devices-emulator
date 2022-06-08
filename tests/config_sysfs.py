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
IF_NET = 'net'

IFACES = (IF_I2C, IF_TTY, IF_NET)

EXTERN_DEV_NAME_PREFIXES = { IF_I2C: 'i2c-', IF_TTY: 'ttyUSB', IF_NET: 'eth' }

KMOD_NAME = 'hwemu'

SYSFS_BASE_DIR = '/sys/kernel/' + KMOD_NAME

# Maximum length of a request
HWE_MAX_REQUEST = (4096 - 1) // 4

# Maximum length of a response
HWE_MAX_RESPONSE = (4096 - 1) // 4

# Minimum number of key-value pairs that can be added to a device
HWE_MIN_PAIRS = 0

# Maximum number of key-value pairs that can be added to a device
HWE_MAX_PAIRS = 1000

# Maximum number of devices per interface
HWE_MAX_DEVICES = 256

# ----------------------------------------------------------------------

def throw(msg):
    raise Exception(msg)

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
        req = rand_hex_str(HWE_MAX_REQUEST)
        if not req in _used_requests:
            _used_requests.append(req)
            return req + '=' + rand_hex_str(HWE_MAX_RESPONSE)

# ----------------------------------------------------------------------

def rand_pair_str_list():
    return { i: rand_pair_str() for i in rand_range(HWE_MIN_PAIRS, HWE_MAX_PAIRS - 1) }

# ----------------------------------------------------------------------

def rand_dev_list(pfx):
    return { '%s%d' % (pfx, i) : rand_pair_str_list() for i in \
        rand_range(1, HWE_MAX_DEVICES) }

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
            pair_nums = sorted((i for i in dev.keys() if isinstance(i, int)))

            for pair_num in pair_nums:
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

def dev_name_to_iface(s):
    for ifc in IFACES:
        if s == ifc or \
           (s.find(ifc) == 0 and s[len(ifc) - len(s):].isdigit()):
            return ifc
    return None

# ----------------------------------------------------------------------

def is_dev_name(s):
    return dev_name_to_iface(s) is not None

# ----------------------------------------------------------------------

def extern_dev_name_to_iface(s):
    for ifc, pfx in EXTERN_DEV_NAME_PREFIXES.items():
        if s.startswith(pfx):
            return ifc
    return None

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
            throw('written only %d byte(s) of %d to file %s' %
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
    # XXX we cannot detect built-in modules this way
    with open('/proc/modules', 'rt') as f:
        for line in f.readlines():
            if module in line:
                return True
    return False

# ----------------------------------------------------------------------

I2C_DEV_DIR = '/sys/class/i2c-dev'

def i2c_get_adapter_name(dev_name):
    return read_file('/'.join([I2C_DEV_DIR, dev_name, 'name']))

# ----------------------------------------------------------------------

def is_i2c_dev_loaded():
    return os.path.isdir(I2C_DEV_DIR)

# ----------------------------------------------------------------------

HWEMU_TTY_NAME = '/dev/ttyHWE'
HWEMU_NETDEV_NAME = 'hwenet'

def ifaces_init(config):

    symlinks = {}
    netdevs = []

    def on_dev(iface_name, dev_name):
        nonlocal symlinks, netdevs
        lnk = config[iface_name][dev_name].get('_extern_dev_name')

        if lnk is None:
            throw('Broken config: No name for device %s' % (dev_name))

        if iface_name == IF_NET:
            netdevs.append(lnk)
        else:
            symlinks[dev_name] = { 'link': '/dev/' + lnk }

    traverse_config(config, on_iface = None, on_dev = on_dev, on_pair = None)

    # i2c

    ifc = IF_I2C

    # We cannot access i2c devices from userspace without the i2c-dev
    # driver loaded.

    if not is_i2c_dev_loaded():
        run(['modprobe', 'i2c_dev'])

    sstr = 'adapter '

    # Collect our /dev/i2c* device names from /sys/class/i2c-dev

    with os.scandir(I2C_DEV_DIR) as it:
        for e in it:
            if e.is_dir():
                fstr = i2c_get_adapter_name(e.name)
                if KMOD_NAME in fstr:
                    n = fstr.find(sstr)
                    if n >= 0:
                        dev_num = fstr[n + len(sstr):].rstrip()
                        dev_name = ifc + dev_num
                        if dev_name in symlinks:
                            symlinks[dev_name]['target'] = '/dev/' + e.name
                        #else:
                        #    throw('Unexpected device /dev/%s' % (e.name))

    # tty

    import glob

    ifc = IF_TTY

    for df in sorted(glob.glob(HWEMU_TTY_NAME + '*'), key = lambda d: int(d[len(HWEMU_TTY_NAME):])):
        dev_num = df[len(HWEMU_TTY_NAME):]
        dev_name = ifc + dev_num
        if dev_name in symlinks:
            symlinks[dev_name]['target'] = df
        #else:
        #    throw('Unexpected device /dev/%s' % (df))

    # create symlinks
    for dev_name, d in symlinks.items():
        lnk = d['link']
        tgt = d.get('target')
        if tgt is None:
            # no device to symlink to
            throw("Couldn't create device %s" % (lnk))
        if lnk == tgt:
            # We don't create a symlink, if the target has the same name.
            # This may happen e.g. for i2c devices.
            continue
        if os.path.exists(lnk):
            pfx = EXTERN_DEV_NAME_PREFIXES[dev_name_to_iface(dev_name)]
            n = 0
            while True:
                suggestion = '%s%d' % (pfx, n)
                if not os.path.exists('/dev/' + suggestion):
                    throw(("Can't create device '%s'\n" +
                        "    Try to use another name, for example '%s'")
                        % (lnk[len('/dev/'):], suggestion))
                n += 1

        os.symlink(tgt, lnk)

    # net

    for n in range(len(netdevs)):
        if not os.path.isdir('/sys/class/net/%s%d' % (HWEMU_NETDEV_NAME, n)):
            throw("Couldn't create device %s" % (netdevs[n]))

        # Bring the interface online.

        # XXX Assume iproute2 is installed on your system (on most
        # systems, it is). Otherwise, we could implement this via
        # ioctl SIOCGIFFLAGS/SIOCSIFFLAGS, e.g:
        # + https://www.oreilly.com/library/view/python-cookbook/0596001673/ch07s05.html
        # + https://stackoverflow.com/questions/20420937/how-to-assign-ip-address-to-interface-in-python
        # + https://stackoverflow.com/questions/6067405/python-sockets-enabling-promiscuous-mode-in-linux

        run(['ip', 'link', 'set', HWEMU_NETDEV_NAME + str(n), 'up'])

# ----------------------------------------------------------------------

def ifaces_cleanup():

    import glob

    # i2c

    # Earlier on we ensured that i2c-dev was loaded and now we have
    # no way of knowing whether or not it had been loaded before. So we
    # just leave it loaded :(
    # Besides, we assume that this function may be called in cmd_start,
    # that is BEFORE i2c-dev is first loaded.

    # remove all symlinks linking to our devices
    for lnk in glob.glob('/dev/' + EXTERN_DEV_NAME_PREFIXES[IF_I2C]  + '*'):
        if os.path.islink(lnk):
            fn = os.path.realpath(lnk)
            if fn.startswith('/dev/i2c-') and KMOD_NAME in i2c_get_adapter_name(fn[len('/dev/'):]):
                os.remove(lnk)

    # tty

    # remove all symlinks linking to our devices
    for lnk in glob.glob('/dev/' + EXTERN_DEV_NAME_PREFIXES[IF_TTY]  + '*'):
        if os.path.islink(lnk):
            tgt = os.path.realpath(lnk)
            if HWEMU_TTY_NAME in tgt:
                os.remove(lnk)

    # net
    # Do nothing, all interfaces will be shut down in the usual way.

# ----------------------------------------------------------------------

def run(args):
    p = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    p.wait()
    if p.returncode == 0:
        return
    err = p.stdout.read().decode().rstrip()
    if err == '':
        err = 'Command terminated with exit code %d: %s' % (p.returncode, ' '.join(args))
    throw(err)

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

    if is_module_loaded(KMOD_NAME):
        # The kernel module is already loaded. We will reload it
        # to ensure the purity of the experiment.
        run(['rmmod', KMOD_NAME])

    run(['insmod', '../kernel/' + KMOD_NAME + '.ko'])

    random.seed()

    # XXX when these values are large, this test can be extremely slow

    global HWE_MAX_DEVICES

    if HWE_MAX_DEVICES > 16:
        HWE_MAX_DEVICES = 16

    global HWE_MAX_PAIRS

    if HWE_MAX_PAIRS > 300:
        HWE_MAX_PAIRS = 300

    print('WARNING: With large maximum values, creating random configs may take a VERY long time.')

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

