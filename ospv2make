# Use the name of your C file here (without .c)
MODULE_NAME = sys_health_monitor

# Standard kernel module Makefile content
obj-m += $(MODULE_NAME).o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# Optional: Commands to load/unload easily (use with 'make load' or 'make unload')
load:
	sudo insmod $(MODULE_NAME).ko
unload:
	sudo rmmod $(MODULE_NAME) || true
