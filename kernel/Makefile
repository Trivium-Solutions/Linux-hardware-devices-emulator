obj-m += vcpsim.o

vcpsim-objs += vs_main.o
vcpsim-objs += vs_sysfs.o
vcpsim-objs += vs_utils.o
vcpsim-objs += vs_tty.o
vcpsim-objs += vs_i2c.o

# use these to turn on debug messages in particular files
#CFLAGS_vs_sysfs.o := -DDEBUG
#CFLAGS_vs_tty.o := -DDEBUG
#CFLAGS_vs_i2c.o := -DDEBUG

# use these to add any global flags
#MY_FLAGS += -g -DDEBUG
#ccflags-m = ${MY_FLAGS}
#CC += ${MY_FLAGS}

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

