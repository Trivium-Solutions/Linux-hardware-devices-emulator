#!/usr/bin/env python3
'''
    Control Utility
'''
import sys
import os
import subprocess
import configparser

PROG_NAME = os.path.splitext(os.path.basename(__file__))[0]
PROG_DIR = os.path.dirname(os.path.realpath(__file__))

_IMPORT_DIRS = ('../tests',)

for d in _IMPORT_DIRS:
    p = os.path.realpath('/'.join((PROG_DIR, d)))
    if os.path.isdir(p):
        sys.path.append(p)

import config_sysfs as config

# ----------------------------------------------------------------------

def throw(msg):
    raise Exception(msg)

# ----------------------------------------------------------------------

def assert_root():
    if os.geteuid() != 0:
        throw('You need to have root privileges to run this command')

# ----------------------------------------------------------------------

def get_module_filename(modname):
    modname += '.ko'
    dirs = ('.', '../kernel')
    for d in dirs:
        p = os.path.realpath('/'.join((PROG_DIR, d, modname)))
        if os.path.isfile(p):
            return p
    dir_lst = ''
    for d in dirs:
        dir_lst += '    %s\n' % (os.path.realpath(d))
    throw('Module %s was not found in any of these directories:\n%s' % (modname, dir_lst))

# ----------------------------------------------------------------------

def insmod(modname):
    config.run(['insmod', modname])

# ----------------------------------------------------------------------

def rmmod(modname):
    config.run(['rmmod', modname])

# ----------------------------------------------------------------------

def is_module_loaded():
    return config.is_module_loaded(config.KMOD_NAME)

# ----------------------------------------------------------------------

def load_module():
    insmod(get_module_filename(config.KMOD_NAME))

# ----------------------------------------------------------------------

def unload_module():
    rmmod(config.KMOD_NAME)

# ----------------------------------------------------------------------

def load_from_ini(filename):

    def error(msg):
        throw('In file %s: %s' % (filename, msg))

    def is_quoted(s):
        return s.startswith('"') and s.endswith('"')

    def convert_quoted(s):
        if not is_quoted(s):
            return s
        return config.bytes_to_hex_str(bytes(s[1: -1], encoding = 'utf8'))

    ini = configparser.ConfigParser()
    lst = ini.read(filename)

    if len(lst) == 0:
        throw('%s: File not found' % (filename))

    for s in ini.sections():
        if not config.is_dev_name(s):
            error('Unsupported device: %s' % (s));

    cfg = {}

    for ifc in config.IFACES:
        cfg[ifc] = {}
        dev_num = 0
        if ini.has_section(ifc):
            # dev0 == dev
            if ini.has_section(ifc + '0'):
                error('Configuration has both %s and %s0' % (ifc, ifc))
            ini[ifc + '0'] = ini[ifc]
            ini.remove_section(ifc)
        while True:
            dev_name = ifc + str(dev_num)
            if ini.has_section(dev_name):
                i = 0
                pairs = {}
                for k, v in ini[dev_name].items():
                    k2 = convert_quoted(k)
                    v2 = convert_quoted(v)

                    # Do some checking. The kernel module won't
                    # let a bad string pass anyway, but the error
                    # message may be a bit cryptic.

                    if not config.is_hex_str(k2):
                        error('Invalid character in request: %s' % (k))

                    if len(k2) > config.HWE_MAX_REQUEST * 2:
                        error('Request string too long: %s' % (k))

                    if (len(k2) & 1) != 0:
                        error('Odd number of characters in request string: %s' % (k))

                    if is_quoted(k) and k2 in ini[dev_name].keys():
                        # quoted string has an equal byte representation
                        error('Duplicate key: %s' % (k))

                    if len(v2) < 1:
                        error('Empty response string for request: %s' % (k))

                    if not config.is_hex_str(v2):
                        error('Invalid character in response: %s' % (v))

                    if len(v2) > config.HWE_MAX_RESPONSE * 2:
                        error('Response string too long: %s' % (v))

                    if (len(v2) & 1) != 0:
                        error('Odd number of characters in response string: %s' % (v))

                    pairs[i] = k2 + '=' + v2
                    i += 1
                cfg[ifc][dev_name] = pairs
                ini.remove_section(dev_name)
                dev_num += 1
            else:
                break

    for k in ini:
        if k != 'DEFAULT':
            error('Device %s has an non-consecutive number' % (k))

    return cfg

# ----------------------------------------------------------------------

def cmd_help():
    print('''
Usage:

    %s <command> [arg]

Commands:
''' % (PROG_NAME))

    for cmd in _OPTS:
        o = _OPTS[cmd]
        if o.get('hide', False):
            continue
        s = '    ' + cmd
        arg = o.get('arg', '')
        if arg != '':
            s += ' <%s>' % arg
        print(s)
        s = o.get('descr', '<No description yet>')
        print('        ' + s)
        print()

# ----------------------------------------------------------------------

def cmd_start(filename):
    assert_root()

    if is_module_loaded():
        config.ifaces_cleanup()
        unload_module()

    load_module()

    cfg = load_from_ini(filename)

    config.write_config(cfg)

    config.ifaces_init(cfg)

# ----------------------------------------------------------------------

def cmd_stop():
    assert_root()

    config.ifaces_cleanup()

    if is_module_loaded():
        unload_module()

# ----------------------------------------------------------------------

def cmd_debug(filename):

    assert_root()

    load_module()

    cfg = load_from_ini(filename)
    #print(config.config_to_str(cfg))

    config.write_config(cfg)

# ----------------------------------------------------------------------

def cmd_random_ini(filename):
    if os.path.exists(filename):
        throw('%s: file already exists' % (filename))

    cfg = config.rand_config()
    txt = ''
    empty_line = False

    def on_dev(iface_name, dev_name):
        nonlocal txt, empty_line
        txt += '%s[%s]\n' % (empty_line and '\n' or '', dev_name)
        empty_line = True

    def on_pair(iface_name, dev_name, pair_num, pair):
        nonlocal txt
        txt += '%s\n' % (pair)

    config.traverse_config(cfg, on_iface = None, on_dev = on_dev, on_pair = on_pair)

    config.write_file(filename, txt)

# ----------------------------------------------------------------------

def cmd_check_ini(filename):
    cfg = load_from_ini(filename)

# ----------------------------------------------------------------------

_OPTS = {
    'help': {
        'fn': cmd_help,
        'descr': 'Display this information.',
    },
    'start': {
        'fn': cmd_start,
        'arg': 'filename',
        'descr': 'Start the emulator using the configuration '+
                 'provided in <filename>.',
    },
    'stop': {
        'fn': cmd_stop,
        'descr': 'Stop the emulator.',
    },

    # undocumented commands (debugging, testing, etc)
    # DO NOT USE!

    'debug': {
        'fn': cmd_debug,
        'arg': 'filename',
        'hide': True,
    },
    'random-ini': {
        'fn': cmd_random_ini,
        'arg': 'filename',
        'hide': True,
    },
    'check-ini': {
        'fn': cmd_check_ini,
        'arg': 'filename',
        'hide': True,
    },
}

def main(argv):

    if len(argv) > 1:
        cmd = argv[1]
        if cmd in _OPTS:
            o = _OPTS[cmd]
            fn = o['fn']
            arg = o.get('arg', '')
            if arg != '':
                if len(argv) < 3:
                    throw(('This command requires an additional ' +
                        'parameter <%s>, which was not specified; run ' +
                        '"%s help" for help') % (arg, PROG_NAME))
                fn(argv[2])
            else:
                fn()
        else:
            throw('Unknown command: %s; run "%s help" for help' %
                (cmd, PROG_NAME))
    else:
        cmd_help()

# ----------------------------------------------------------------------

def cleanup():
    if is_module_loaded():
        unload_module()

# ----------------------------------------------------------------------

# TODO make this assignable with a command-line option?
_DEBUG = False
#_DEBUG = True

_ERROR_EXIT_CODE = 1

if _DEBUG:
    import traceback

if __name__ == '__main__':
    exit_code = _ERROR_EXIT_CODE

    try:
        main(sys.argv)
        exit_code = 0
    except Exception as ex:
        # don't show the traceback when we're not debugging
        if _DEBUG:
            cleanup()
            print(traceback.format_exc())
        else:
            # if something goes wrong, try to clean up the mess
            # ignoring new errors
            try: cleanup()
            except: pass

            print('*** ERROR: ' + str(ex))

    sys.exit(exit_code)
