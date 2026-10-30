KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += cca_driver.o
cca_driver-y := cca.o cca-dma-helper.o

ccflags-y := -Werror

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

.PHONY: all clean modules_install
