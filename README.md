# emulator

Emulator of arbitrary Linux devices.

This software consists of two major components: a Linux kernel module
and a control utility. The kernel module implements drivers for various
devices, which act as real hardware devices capable of exchanging data
with a user-space application. The data exchange is done in the form of
request-response, where the user-space application sends packets of data
to the emulated device and receives packets of data in reply, according
to configuration.

Currently, the kernel module aims to emulate the following types of
devices:
- tty
- i2c
- network

The control utility is responsible for configuring the kernel module.

## Installation and usage

The installation procedure is quite simple:

```
$ git clone https://github.com/Trivium-Solutions/emulator hwemu
$ cd hwemu
$ sudo ./install
```

However, you should be aware that the installation process involves
building the kernel module. It is necessary to build the kernel module
on your machine because Linux won't allow you to load a module built
for a different kernel version. The installer program may warn
you of some missing packages which you must install to continue the
installation.

When the installation is complete, you can load a test configuration
using the control utility:

```
$ hwectl start tests/test.ini
```

The control utility requires root privileges to operate, so you may be
prompted for the root password.

If the control utility encounters a device naming conflict, the
configuration will not be loaded, and an error message will be printed:

```
*** ERROR: Can't create device 'i2c-0'
    Try to use another name, for example 'i2c-2'
```

In this case, you should replace the device name specified in your
`.ini` file (see [Configuration syntax](#configuration-syntax)).

To stop emulation, use the `stop` command:

```
$ hwectl stop
```

This command will remove the emulated devices and unload the kernel
module.

To get a list of available commands, run:

```
$ hwectl help
```

## Configuration syntax

Configuration files have an
[INI](https://en.wikipedia.org/wiki/INI_file)-like syntax. The sections
of a configuration file represent the devices you want to emulate. The
section names specify the device names that will be created in `/dev`
(except for network devices). For example, the section name `[ttyUSB0]`
instructs the emulator to create the device `/dev/ttyUSB0` (if such a
device doesn't already exist).

The section names must match certain patterns:

- `ttyUSB*` for TTY devices;
- `i2c-*` for I2C devices.
- `eth*` for network devices.

Any other names are considered invalid.

The key-value pairs of the sections represent request and response
data. Each byte of the data is represented as a string value of the
form `xx`, where `x` is a hexadecimal number, e.g. `1A`. All the data
bytes are represented as a consecutive string of such values, e.g.
`1A2B3C`.

Thus, if a request has the bytes `00` `01` `03` `04` and the response
is `AA` `BB` `CC`, you should add the following key-value pair to the
corresponding device section:

```
[ttyUSB0]
00010304=AABBCC
```

Optionally, instead of hexadecimal values, you can use quoted string
values. For example, if your request has the string value
`"r80000000"`, you don't need to convert it to `723830303030303030`, you
can just use the string value in quotes. In this case, your
request-response pair may look like this:


```
[ttyUSB0]
"r80000000"="r80000000=12345678"
```

A simple example configuration is in the file [tests/test.ini](/tests/test.ini).
