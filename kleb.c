#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h> // high res timer
#include <linux/ktime.h> // ktime representation
#include <linux/math64.h> // div_u64
#include <linux/slab.h> // kmalloc
#include <linux/device.h> // character devices
#include <linux/fs.h> // file control
#include <linux/cdev.h>
#include <linux/version.h> // linux version
#include <asm/uaccess.h>
#include <asm/nmi.h> // reserve_perfctr_nmi ...
#include <asm/perf_event.h> // union cpuid10...
#include <asm/special_insns.h> // read and write cr4
#include "kleb.h"

#include <linux/kprobes.h> // kprobe and jprobe

// DEBUG
#include <linux/time.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	#define UNLOCKED 1
#endif

MODULE_LICENSE("GPL"); // Currently using the MIT license
MODULE_AUTHOR("James Bruska");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.6.0");

static struct hrtimer hr_timer;
static ktime_t ktime_period_ns;
static int delay_in_ns, num_events, num_recordings, counter, timer_restart;
static int target_pid, recording;
static int** hardware_events;
static int Major;
static lprof_cmd_t lprof_cmd;

struct cdev *kernel_cdev;


//DEBUG
static struct timespec ts1, ts2;







unsigned long rdpmc_instructions(void)
{
	unsigned a, d, c;

	c = (1<<30)+1;
	__asm__ volatile( "rdpmc" : "=a" (a), "=d" (d) : "c" (c) );

	return ((unsigned long) a) | (((unsigned long) d) << 32);;
}

unsigned long start_rdpmc_ins, stop_rdpmc_ins;
long int count_in;

static long pmu_start_counters(unsigned int counter, unsigned long long config)
{
	int reg_addr = 0x186; // 0x186 is perfeventsel0
	//int event_no=0x4f2e; 
	//int event_no=0x412e; 
	//int event_no=0xc0; 
	int event_no = counter;
	int umask = 0x3F00; 
	int enable_bits = 0x430000; 
	int event=event_no | umask | enable_bits;

    long int eax_low, edx_high;
    int reg_addr_val = 0xc1; // MSR_CORE_PERF_GLOBAL_CTRL; // 0xc1 is perfctr0

	// DEBUG
	//printk( "event: %d", event_no);
	getnstimeofday( &ts1 );

	__asm__ ("wrmsr" : : "c"(reg_addr), "a"(event), "d"(0x00));

	__asm__ ("wrmsr" : : "c"(reg_addr_val), "a"(0x00), "d"(0x00));
	__asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr_val));
    count_in = ((long int)eax_low | (long int)edx_high<<32);
	//printk( KERN_INFO "rdmsr in:   %ld", count_in);
	__asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr_val));
    count_in = ((long int)eax_low | (long int)edx_high<<32);
	//printk( KERN_INFO "rdmsr in:   %ld", count_in);

	return count_in;
}

static long pmu_stop_counters( void )
{
	long int count;
    long int eax_low, edx_high;
    int reg_addr = 0xc1; //MSR_CORE_PERF_GLOBAL_CTRL; // 0xc1 is perfctr0 

    __asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr));
    count = ((long int)eax_low | (long int)edx_high<<32);

	printk( KERN_INFO "rdmsr in:   %ld", count_in);
	printk( KERN_INFO "rdmsr out:  %ld", count);
	printk( KERN_INFO "rdmsr diff: %ld", count - count_in);
	
	// DEBUG
	printk( "counter at stop: %d\n", counter );
	getnstimeofday( &ts2 );
	printk( "%ld - \n%ld = \n%ld\n", ts2.tv_nsec, ts1.tv_nsec, ts2.tv_nsec - ts1.tv_nsec);


    return count;
}

static long pmu_reset_counters1(unsigned int counter, unsigned long long config)
{
    long int eax_low, edx_high;
    int reg_addr_val = 0xc1; // MSR_CORE_PERF_GLOBAL_CTRL; // 0xc1 is perfctr0

	__asm__ ("wrmsr" : : "c"(reg_addr_val), "a"(0x00), "d"(0x00));
	__asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr_val));
    count_in = ((long int)eax_low | (long int)edx_high<<32);
	//printk( KERN_INFO "rdmsr reset:   %ld", count_in);
	__asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr_val));
    count_in = ((long int)eax_low | (long int)edx_high<<32);
	//printk( KERN_INFO "rdmsr reset:   %ld", count_in);

	return count_in;
}

static long pmu_read_counters1( void )
{
	long int count;
    long int eax_low, edx_high;
    int reg_addr = 0xc1; //MSR_CORE_PERF_GLOBAL_CTRL; // 0xc1 is perfctr0 

    __asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr));
    count = ((long int)eax_low | (long int)edx_high<<32);

	//printk( KERN_INFO "rdmsr read:   %ld\n", count);
	hardware_events[0][counter] = count; 
	//printk( KERN_INFO "he[i][counter] = %d\n", hardware_events[0][counter] ); 
	
	hardware_events[num_events][counter] = 0;
		
    return count;
}







int kprobes_handle_do_exit(struct kprobe* p, struct pt_regs* regs)
{
	if ( timer_restart ){
  	printk_d("In kprobes_handle_do_exit()\n");
  }
	return 0;
}

int kprobes_handle_copy_process(struct kprobe* p, struct pt_regs* regs)
{
	if ( timer_restart ){
  	printk_d("In kprobes_handle_copy_process()\n");
  }
	return 0;
}

struct rq* jprobes_handle_finish_task_switch(struct task_struct* prev)
{
	int i;
	
	if ( recording && ( counter < num_recordings ) ) { // TODO: rm 2nd cond
		if ( current->pid == target_pid ) { 
			timer_restart = 1;
			ktime_period_ns = ktime_set( 0, delay_in_ns );
			hrtimer_start( &hr_timer, ktime_period_ns, HRTIMER_MODE_REL );
  		printk_d("Timer in: jprobes_handle_finish_task_switch() [%d] -> [%d]\n", prev->pid, current->pid);
			if ( counter < num_recordings ) {	
				for ( i=0; i < num_events; ++i ) {
					 hardware_events[i][counter] = -2; 
  			}
				hardware_events[num_events][counter] = 2;
				++counter;
			}	
			pmu_reset_counters1(lprof_cmd.counter, lprof_cmd.config);
		} else if ( prev->pid == target_pid ) {
			timer_restart = 0; 
			hrtimer_cancel( &hr_timer ); // Needed?
			printk_d("Timer out: jprobes_handle_finish_task_switch() [%d] -> [%d]\n", prev->pid, current->pid);
			for ( i=0; i < num_events; ++i ) {
				 hardware_events[i][counter] = -3; 
  			}
			hardware_events[num_events][counter] = 3;
			++counter;
			pmu_stop_counters();
		}
	}

	jprobe_return();
	//should not get here
	return (NULL);
}

static struct kprobe do_exit_kp = {
  .pre_handler = kprobes_handle_do_exit,
  .symbol_name = DO_EXIT_NAME,
};

static struct kprobe copy_process_kp = {
  .pre_handler = kprobes_handle_copy_process,
  .symbol_name = COPY_PROCESS_NAME,
};

static struct jprobe finish_task_switch_jp = {
  .entry = jprobes_handle_finish_task_switch,
  .kp.symbol_name = "finish_task_switch",
};

void unregister_all(void)
{
  unregister_kprobe(&do_exit_kp);
  unregister_kprobe(&copy_process_kp);
  unregister_jprobe(&finish_task_switch_jp);
}

int register_all(void)
{
//here we will try two things
  // 1. We will try to use a jprobe on "finish_task_switch" so we can access
  //    the prev task pointer that is passed in. We can use that to compare
  //    with current to see where we are.
  // 2. We will also use a kprobe on fork or "copy_process" which we will just
  //    assume works fine.
  int ret = register_kprobe(&do_exit_kp);
  if (ret < 0)
  {
    printk(KERN_INFO "Couldn't register 'do_exit' kprobe\n");
    unregister_all();
    return (-EFAULT);
  }
 
  //TODO: consider removing copy_process in the first place.
  ret = register_kprobe(&copy_process_kp);
  if (ret < 0)
  {
    //On one of my machines, the name for copy_process in kallsyms was copy_process.
    //  part.27 -- it had a suffix. An alternative would be uprobe_copy_process 
    //  which is called at the end of the copy_process function on success.
    printk(KERN_INFO "Couldn't register 'copy_process' kprobe\n");
    printk(KERN_INFO "  Trying to register 'uprobe_copy_process' kprobe\n");
    copy_process_kp.symbol_name = UPROBE_COPY_PROCESS_NAME;
    ret = register_kprobe(&copy_process_kp);
    if (ret < 0)
    {
      printk(KERN_INFO "  That didn't work either\n");
      unregister_all();
      return (-EFAULT);
    }
  }

  ret = register_jprobe(&finish_task_switch_jp);
  if (ret < 0)
  {
    printk(KERN_INFO "Couldn't register 'finish_task_switch' jprobe\n");
    unregister_all();
    return (-EFAULT);
  }

  printk_d("lprof: finish_task_switch @ [%p], do_exit @ [%p], copy_process @ [%p]\n", finish_task_switch_jp.kp.addr, do_exit_kp.addr, copy_process_kp.addr);

  return (0);
}








enum hrtimer_restart hrtimer_callback( struct hrtimer *timer ) {
	ktime_t kt_now;
	int overrun;	

	if ( timer_restart && (counter < num_recordings) ) {	
		pmu_read_counters1();

		++counter;
		kt_now = hrtimer_cb_get_time( &hr_timer );
		overrun = hrtimer_forward( &hr_timer, kt_now, ktime_period_ns );
		
		return HRTIMER_RESTART;
	} else {
		if ( !timer_restart ) {
			printk( "Timer Expired\n" );
		} else {
			printk( "Counter > allowed spaces: %d > %d\n", counter, num_recordings );
		}
		return HRTIMER_NORESTART;
	}
}

int open( struct inode *inode, struct file *fp ) {
	printk( KERN_INFO "Inside open\n" );
	return 0;
}

ssize_t read( struct file *filep, char *buffer, 
                   size_t len, loff_t *offset ) {
	int size_of_message = num_recordings * (num_events+1) * sizeof(int);
	int error_count = copy_to_user( buffer, hardware_events[0], 
                                   size_of_message );

  if ( error_count==0 ){
    printk( KERN_INFO "Sent %d characters to the user\n", size_of_message );
    return ( size_of_message==0 );
  } else {
    printk( KERN_INFO "Failed to send %d characters to the user\n", error_count );
    return -EFAULT;
  }
}

int release( struct inode *inode, struct file *fp ) {
	printk( KERN_INFO "Inside close\n" );
	return 0;
}

int start_counters( unsigned int pmu_counter, unsigned long long pmu_config ) {
	int i, j;

	if ( !recording ){
		recording = 1;
		timer_restart = 1;
		counter = 0;
		ktime_period_ns = ktime_set( 0, delay_in_ns );
		for ( i=0; i < (num_events+1); ++i ) { // reset values
			for ( j=0; j < num_recordings; ++j ) {
				hardware_events[i][j] = -420;
			}
		}
		pmu_start_counters(pmu_counter, pmu_config);
		hrtimer_start( &hr_timer, ktime_period_ns, HRTIMER_MODE_REL );
	} else {
		printk( KERN_INFO "Invalid action: Counters already collecting\n" );
	}

	return 0;
}

int stop_counters() {
	pmu_stop_counters();
	recording = 0;
	timer_restart = 0;

	return 0;
}

#ifdef UNLOCKED

long ioctl_funcs( struct file *fp, unsigned int cmd, unsigned long arg ) {
	int ret = 0;

	lprof_cmd_t* lprof_cmd_user = (lprof_cmd_t*)(arg);

	if (lprof_cmd_user == NULL)
	{ 
		printk_d("lprof_ioctl: User did not pass in cmd\n");
		return (-EINVAL);
	}

	//copy the values from userspace
	if (copy_from_user(&lprof_cmd, lprof_cmd_user, sizeof(lprof_cmd_t)) != 0)
	{
  		printk_d("lprof_ioctl: Could not copy cmd from userspace\n");
  		return (-EINVAL);
	}

	switch( cmd ) {
		case IOCTL_DEFINE_COUNTERS:
			printk( KERN_INFO "This will define the counters\n" );
			break;
		case IOCTL_START:
			printk( KERN_INFO "Starting counters\n" );
			target_pid = lprof_cmd.pid;
			printk( KERN_INFO "target pid: %d\n", (int) target_pid );
			start_counters(lprof_cmd.counter, lprof_cmd.config);
			//pmu_start_counters(lprof_cmd.counter, lprof_cmd.config);
			break;
		case IOCTL_STOP:
			printk( KERN_INFO "Stopping counters\n" );
			stop_counters();
			//pmu_stop_counters();
			break;
		case IOCTL_DELETE_COUNTERS:
			printk( KERN_INFO "This will delete the counters\n" );
			break;
		case IOCTL_DEBUG:
			printk( KERN_INFO "This will set up debug mode\n" );
			break;
		case IOCTL_STATS:
			printk( KERN_INFO "This will set up profiling mode\n" );
			break;
		default:
			printk( KERN_INFO "Invalid command\n" );
			break;
	}

	return ret;
}

struct file_operations fops = {
	open: open,
	read: read,
	unlocked_ioctl: ioctl_funcs,
	release: release
};

#else // TODO: Fix this to match the upper one

int ioctl_funcs( struct inode *inode, struct file *fp,
                  unsigned int cmd, unsigned long arg ) {

}

struct file_operations fops = {
	open: open,
	read: read,
	ioctl: ioctl_funcs,
	release: release
};

#endif

int initialize_memory() {
	int i, j;
	
	printk( "Memory initializing\n" );
	
	delay_in_ns = 1E5L;
	num_events = 4;
	num_recordings = div_u64(1E8L, delay_in_ns); // Holds 100ms worth of data
	
	hardware_events = kmalloc( (num_events+1)*sizeof(int *), GFP_KERNEL );
	hardware_events[0] = kmalloc( (num_events+1)*num_recordings*sizeof(int), GFP_KERNEL );
	for ( i=0; i < (num_events+1); ++i ) { // This reduces the number of kmalloc calls
		hardware_events[i] = *hardware_events + num_recordings * i;
		for ( j=0; j < num_recordings; ++j ) {
			hardware_events[i][j] = -420;
		}
	}
	
	return 0;
}

int initialize_timer() {
	printk( "Timer initializing\n" );
	
	counter = 0;
	timer_restart = 0;

	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &hrtimer_callback;
	
	return 0;
}

int initialize_ioctl() {
	int ret;
	dev_t dev_no, dev;
	
	printk( "IOCTL initializing\n" );

	kernel_cdev = cdev_alloc();
	kernel_cdev->ops = &fops;
	kernel_cdev->owner = THIS_MODULE;

	ret = alloc_chrdev_region( &dev_no, 0, 1, "char_arr_dev" );
	if( ret < 0 ){
		printk( "Major number allocation has failed\n" );
		return ret;
	}

	Major = MAJOR( dev_no );
	dev = MKDEV( Major, 0 );
	printk( "The major number for your device is %d\n", Major );

	ret = cdev_add( kernel_cdev, dev, 1 );
	if( ret < 0 ){
		printk( KERN_INFO "Unable to allocate cdev" );
		return ret;
	}

	return 0;
}

int init_module( void ) {	
	int ret;

	if ( initialize_memory() < 0 ){ 
		printk( KERN_INFO "Memory failed to initialize" );
		return (-ENODEV); 
	}

	if ( initialize_timer() < 0 ){ 
		printk( KERN_INFO "Timer failed to initialize" );
		return (-ENODEV); 
	}

	if ( initialize_ioctl() < 0 ){ 
		printk( KERN_INFO "IOCTL failed to initialize" );
		return (-ENODEV); 
	}

	ret = register_all();
  if (ret != 0)
  {
    return (ret);
  }

	printk( "Starting timer to callback in %dns\n", delay_in_ns );	

	printk( "K-LEB module initialized\n" );
	return 0;
}

int cleanup_memory() {
	printk( "Memory cleaning up\n" );
	
	kfree(hardware_events[0]);
	kfree(hardware_events);

	return 0;
}

int cleanup_timer() {
	int ret;
	
	printk( "Timer cleaning up\n" );

	ret = hrtimer_cancel( &hr_timer );
	if ( ret ) printk( "The timer was still in use...\n" );

	return 0;	
}

int cleanup_ioctl() {
	printk( "IOCTL cleaning up\n" );

	cdev_del(kernel_cdev);
  unregister_chrdev_region(Major, 1);	

	return 0;
}

void cleanup_module( void )
{
	if ( cleanup_memory() < 0 ){ 
		printk( KERN_INFO "Memory failed to cleanup cleanly" );
	}

	if ( cleanup_timer() < 0 ){ 
		printk( KERN_INFO "Timer failed to cleanup cleanly" );
	}

	if ( cleanup_ioctl() < 0 ){ 
		printk( KERN_INFO "IOCTL failed to cleanupcleanly" );
	}

	unregister_all();

	printk( "K-LEB module uninstalled\n" );

	return;
}




