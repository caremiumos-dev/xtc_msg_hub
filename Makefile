ifneq ($(KERNELRELEASE),)
obj-m += xtc_msg_hub.o
xtc_msg_hub-y := src/xtc_msg_hub.o
else

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

.PHONY: all modules clean install

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

install: modules
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install

endif
