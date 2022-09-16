obj-m += hwemu.o

hwemu-objs += hwe_main.o
hwemu-objs += hwe_sysfs.o
hwemu-objs += hwe_ioctl.o
hwemu-objs += hwe_utils.o
hwemu-objs += hwe_tty.o
hwemu-objs += hwe_i2c.o
hwemu-objs += hwe_net.o
hwemu-objs += hwe_spi.o
hwemu-objs += hwe_async.o

# use these to turn on debug messages in particular files
#CFLAGS_hwe_sysfs.o := -DDEBUG
#CFLAGS_hwe_ioctl.o := -DDEBUG
#CFLAGS_hwe_tty.o := -DDEBUG
#CFLAGS_hwe_i2c.o := -DDEBUG
#CFLAGS_hwe_spi.o := -DDEBUG
#CFLAGS_hwe_async.o := -DDEBUG

# use these to add any global flags
#MY_FLAGS += -g -DDEBUG
#ccflags-m = ${MY_FLAGS}
#CC += ${MY_FLAGS}

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

