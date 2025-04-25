# Makefile for the System Health Monitor Kernel Module
# This Makefile is generally architecture-independent.
# Target architecture is specified when invoking 'make'.

# Specify the name of the module object file (without .o)
MODULE_NAME=sys_health_monitor

# Tell kbuild the name of the final module object(s) to build.
obj-m := $(MODULE_NAME).o

# Path to the kernel source/build directory.
# IMPORTANT: For cross-compiling, this MUST point to the kernel source
# tree that has been configured and built for your TARGET ARM architecture.
# You might need to set this explicitly, e.g., KDIR=/path/to/arm/kernel/source
KDIR ?= /lib/modules/$(shell uname -r)/build

# PWD is the current working directory (where this Makefile resides).
PWD := $(shell pwd)

# Default target executed when you just run 'make'.
# The ARCH and CROSS_COMPILE variables are passed here if cross-compiling.
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Target to clean up build artifacts.
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Target to install the module (less common in cross-compile scenarios unless
# you have a mounted target filesystem or specific install path).
# You might need to adjust INSTALL_MOD_PATH when cross-compiling.
install: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

# Declare targets that are not actual files.
.PHONY: all clean install
