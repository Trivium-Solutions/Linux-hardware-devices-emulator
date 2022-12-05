# Linux hardware devices emulator

Linux Emulator of arbitrary hardware devices. This software can be used to
emulate hardware devices for testing purposes. It support a request/respose
type of communication (request initiated from userspace, kernel module
returns a response according to configuration) as well as unilateral
transfers from emulated devices to userspace.


This software consists of two major components: a Linux kernel module
and a control utility. The kernel module implements drivers for various
devices, which act as real hardware devices capable of exchanging data
with a user-space application. The data exchange is done in the form of
request-response, where the user-space application sends packets of data
to the emulated device and receives packets of data in reply, according
to configuration. The emulator can also be instructed to periodically
send data packets from the emulated device to the user-space application.

Currently, the kernel module aims to emulate the following types of
devices:
- tty
- i2c
- network
- spi

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

The section names must match the following patterns:

- `ttyUSB*` for TTY devices;
- `i2c-*` for I2C devices;
- `eth*` for network devices;
- `spi-*` for SPI devices.

Any other names are considered invalid.

### Request/response transfer configuration

In the request/response type of transfer, the key-value pairs of the
sections represent request and response data. Each byte of the data is
represented as a string value of the form `xx`, where `x` is a
hexadecimal number, e.g. `1A`. All the data bytes are represented as a
consecutive string of such values, e.g. `1A2B3C`.

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

### Unilateral transfer configuration

The emulated device can be configured to periodically send data packets
to userspace. In this case, the key-part of the key-value pair specifies
the period with which the packet is to be transferred. The value-part
describes the packet data, much as in the request/response type of
transfer.

In this case, the key-value pair has the following syntax:

`timer:`[*hh*`h`[*mm*`m`[*ss*`s`[*ms*`ms`]]]]`=`*data_bytes*

where:

- elements in square brackets are optional;
- *hh*, *mm*, *ss*, and *ms* are the values for hours, minutes, seconds
  and milliseconds respectively; each of these elements is optional but
  at least one element must be present;
- *data_bytes* is the description of the data bytes; it has the same
  format as in the request/response type of transfer.

For example, the following record in a configuration file tells the
emulated device `/dev/ttyUSB0` to "receive" data bytes `AA` `BB` `CC`
every 1 minute 35 seconds and 256 milliseconds.

```
[ttyUSB0]
timer:1m35s256ms=AABBCC
```

### Example

A simple example configuration is in the file [tests/test.ini](/tests/test.ini).

## Limitations

The emulator has certain limitations.

- Although you can specify a period for data reception with precision of
  up to one millisecond, in reality this period cannot be less than a
  [jiffy](https://man7.org/linux/man-pages/man7/time.7.html); this
  restriction is imposed by hardware.
- In configuration files, every key-part of the key-value pair must be
  unique within the section; this is a requirement of the INI file
  syntax.
