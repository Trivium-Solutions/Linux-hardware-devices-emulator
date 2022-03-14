# emulator
Emulator of arbitrary Linux devices.

This software consists of two major components: a Linux kernel module and a control utility. The kernel module implements drivers for various devices, which act as real hardware devices capable of exchanging data with a userspace application. The data exchange is done in the form of request-response, where the userspace application sends packets of data to the emulated device and receives packets of data in reply, according to configuration.

Currently, the kernel module aims to emulate the following types of devices:
- tty
- i2c

The control utility is responsible for configuring the kernel module.
