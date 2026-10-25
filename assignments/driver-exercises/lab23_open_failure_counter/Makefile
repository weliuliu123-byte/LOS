KDIR ?= $(HOME)/embedded_lab1/kernel/linux-6.1.90

ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m += lab2_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
