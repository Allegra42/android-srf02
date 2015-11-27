KERNEL_DIR=~/linaro/pandaboard/

obj-$(CONFIG_SRF02_APP) += srf02.o
obj-m := srf02.o

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD)

clean:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) clean

