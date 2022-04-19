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
    dirs = ['.', '../kernel']
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
    ini = configparser.ConfigParser()
    lst = ini.read(filename)

    if len(lst) == 0:
        throw('%s: file not found' % (filename))

    cfg = {}

    for ifc in config.IFACES:
        cfg[ifc] = {}
        dev_num = 0
        if ini.has_section(ifc):
            # dev0 == dev
            if ini.has_section(ifc + '0'):
                throw('Configuration has both %s and %s0' % (ifc, ifc))
            ini[ifc + '0'] = ini[ifc]
            ini.remove_section(ifc)
        while True:
            dev_name = ifc + str(dev_num)
            if ini.has_section(dev_name):
                i = 0
                pairs = {}
                for k, v in ini[dev_name].items():
                    # TODO handle TXT: lines
                    pairs[i] = k + '=' + v
                    i += 1
                cfg[ifc][dev_name] = pairs
                ini.remove_section(dev_name)
                dev_num += 1
            else:
                break

    for k in ini:
        if k != 'DEFAULT':
            throw('Device %s has an non-consecutive number' % (k))

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
        unload_module()

    load_module()

    cfg = load_from_ini(filename)

    config.write_config(cfg)

    # TODO interface-specific init

# ----------------------------------------------------------------------

def cmd_stop():
    assert_root()

    # TODO interface-specific clean-up

    if is_module_loaded():
        unload_module()

# ----------------------------------------------------------------------

def cmd_debug(filename):

    assert_root()

    load_module()

    cfg = load_from_ini(filename)

    config.write_config(cfg)

# ----------------------------------------------------------------------

def cmd_random_ini(filename):
    pass

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

    'debug': {
        'fn': cmd_debug,
        'arg': 'filename',
        'hide': True,
    },
    'random_ini': {
        'fn': cmd_random_ini,
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
            if o.get('arg', '') != '':
                if len(argv) < 3:
                    throw('This command requires an additional ' +
                        'parameter, which was not specified; run ' +
                        '"%s help" for help' % (PROG_NAME))
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
_DEBUG = True

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
