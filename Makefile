obj-m := kleb.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

OBJS := ioctl_start.o 
CC := gcc
CFLAGS := 

all: kleb_module ioctl_start 
	

kleb_module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 

ioctl_start: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $<

#ioctl_stop: $(OBJS)
#	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean: kleb_module_clean ioctl_start_clean 
	

.PHONY: kleb_module_clean
kleb_module_clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: ioctl_start_clean
ioctl_start_clean:
	$(RM) *.o ioctl_start



