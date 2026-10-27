obj-m += sms_sensor_driver.o

KDIR := $(HOME)/embedded_lab1/kernel/linux-6.1.90
PWD  := $(shell pwd)

ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) \
		M=$(PWD) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		modules

clean:
	$(MAKE) -C $(KDIR) \
		M=$(PWD) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		clean