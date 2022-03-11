obj-m += vcpsim.o

vcpsim-objs += vs_main.o
vcpsim-objs += vs_sysfs.o
vcpsim-objs += vs_tty.o
vcpsim-objs += vs_i2c.o

#CFLAGS_vs_sysfs.o := -DDEBUG

#MY_FLAGS += -g -DDEBUG 
#ccflags-m = ${MY_FLAGS}
#CC += ${MY_FLAGS}

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

