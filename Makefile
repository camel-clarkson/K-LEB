obj-m := kleb.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

OBJS := ioctl_test.o
CC := gcc
CFLAGS := 

all: kleb_module ioctl_test
	

kleb_module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 

ioctl_test: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean: kleb_module_clean ioctl_test_clean
	

.PHONY: kleb_module_clean
kleb_module_clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: ioctl_test_clean
ioctl_test_clean:
	$(RM) *.o ioctl_test



