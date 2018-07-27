# K-LEB
K-LEB (Kernel - Linear Event Batches)

This is a kernel module designed to monitor hardware events from a kernel level.
It implements three key features: 
  1) The ability to send this event info back to user space
  2) The ability to keep the event recording process specific
  3) The ability to monitor events periodically with a high resolution kernel timer

To automatically apply the module, run: 

```
sudo bash initialize.sh
```


To manually apply the module, run the following commands:
	
	
	make clean; make
	sudo insmod kleb.ko
	dmesg
	sudo mknod /dev/kleb c <major number> 0
	

To manually start the module, run the following bash command:
	
	sudo ./ioctl_start <counter> <umask> <timer delay> <./program path>
	
	
To stop the module, run the following bash command:
	
	
	sudo ./ioctl_stop
	

