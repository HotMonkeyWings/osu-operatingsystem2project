obj-m := testMemory.o
KDIR := ../../linux-yocto-3.14

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules


clean:
	rm -f *.o *.ko *.ko.unsigned modules.order Module.symvers testMemory.mod.c .*.cmd

.PHONY: all clean
