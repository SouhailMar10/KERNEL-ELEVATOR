obj-m += elevator.ko

KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD)/src modules
	$(MAKE) -C $(KDIR) M=$(PWD)/src/producer-consumer modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/src clean
	$(MAKE) -C $(KDIR) M=$(PWD)/src/producer-consumer clean
