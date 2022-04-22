# emulator

Emulator of arbitrary Linux devices.

This software consists of two major components: a Linux kernel module
and a control utility. The kernel module implements drivers for various
devices, which act as real hardware devices capable of exchanging data
with a user space application. The data exchange is done in the form of
request-response, where the user space application sends packets of data
to the emulated device and receives packets of data in reply, according
to configuration.

Currently, the kernel module aims to emulate the following types of
devices:
- tty
- i2c

The control utility is responsible for configuring the kernel module.

## Installation and usage

The emulator doesn't require any special installation procedure, except
for building the kernel module. You have to build the kernel module
yourself because Linux won't allow you to load a module built for a
different version of kernel. Building a kernel module in Linux,
however, is not so difficult as it may seem. For example, on Ubuntu you
may need to install just a couple of additional packages:

```bash
$ sudo apt install build-essential linux-headers-`uname -r`
```

This will install building tools and Linux headers. Next, you should
download the emulator code and launch the building process:

```
$ git clone https://github.com/Trivium-Solutions/emulator vcpsim
$ cd vcpsim/kernel
$ make
```

When the kernel module is successfully built, the emulator is ready for
work. Now you can load a test configuration using the control utility:

```
$ cd ../control
$ sudo ./vcpctl start ../tests/test.ini
Available devices:
------------------
i2c0 as /dev/i2c-1
tty0 as /dev/ttyUSB0
```

Note that the control utility printed a list of emulated devices
accessible from user space. The numbers in the /dev-names may be
different on your machine, since they depend on your hardware.

Below is an example of the `VcpSdkCmd` utility interacting with the
emulated TTY device `/dev/ttyUSB0`. The device number `0` is specified
in the `u0` command-line parameter.

```
$ sudo VcpSdkCmd /debug u0 VCPGET 000000000000 0000 FFFF 00010300 8
2022-04-22 10:57:12.709 Sent     >>  1BA27E00000000000000000401C100FFFF000000000C00010300000800001BB0000000
2022-04-22 10:57:12.709 Received <<  1BA27E000000000000000006024100FFFF00000000140001030000080064D0502306000001021BB0
Response: 7E000000000000000006024100FFFF00000000140001030000080064D050230600000102
Response receiving time [us]: 2143
VCP code: 00000000 (OK)
Value: D050230600000102
```

Here is an example of interacting with the emulated I2C device. Note
that the name of our device is `/dev/i2c-1`, which corresponds to the `1`
in the `i10-1` command-line parameter.

**Important: For this example to work, `VcpSdkCmd` must be compiled
without Aardvark support.**

```
$ sudo VcpSdkCmd /debug i10-1 VCPGET 000000000000 0000 FFFF 00010300 8
2022-04-22 10:57:32.714 Sent     >>  7E00000000000000000401C100FFFF000000000C0001030000080000
2022-04-22 10:57:32.714 Received <<  7E000000000000000006024100FFFF00
2022-04-22 10:57:32.714 Received <<  000000140001030000080064D050230600000102
Response receiving time [us]: 114
VCP code: 00000000 (OK)
Value: D050230600000102
```

To stop emulation and unload the kernel module, use the `stop` command:

```
$ sudo ./vcpctl stop
```

To get a list of available commands, run:

```
$ ./vcpctl help
```
