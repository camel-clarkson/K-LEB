# K-LEB (Kernel - Lineage of Event Behavior)
##### This is a kernel module designed to monitor hardware events from a kernel level. 

##### It implements three key features:

1. The ability to send this event info back to user space
2. The ability to keep the event recording process specific
3. The ability to monitor events periodically with a high resolution kernel timer

## Supported Kernel
Linux Kernel 4.13.0-15 and earlier

# Setup

#### Prerequisite 
Install essential development tools and the kernel headers 
```
 apt-get install build-essential linux-headers-$(uname -r)
```

### Automatically spply the module:
-  Run: 
```
sudo bash initialize.sh
```
- select option: 2) Setup
    - The script will automatically compile and insert K-LEB kernel module to the kernel.

### Manually apply the module

- run the following commands:
```
make clean; make
sudo insmod kleb.ko
dmesg
sudo mknod /dev/kleb c <major number> 0
```
# Getting started
Initialize.sh script use configuration file perf.cfg for events selection.
perf.cfg

> \<HPC Event1\>

> \<HPC Event2\>

> \<HPC Event3\>

> \<HPC Event4\>
	
To automatically start monitoring, run:
```
sudo bash initialize.sh
```
select option: 1) Start

Enter timer granularity in ms

Select program to monitor using \<Program PATH\> or \<Program PID\>
		
To manually start monitoring, run the following bash command:
```
sudo ./ioctl_start <Event1> <Event2> <Event3> <Event4> <umask> <timer delay (in ms)> <Log path> <program path>
```

After finish monitoring, HPC data is logged and stored in Output.csv in the current directory or in \<Log path\>
