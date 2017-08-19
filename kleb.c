#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h> // high res timer
#include <linux/ktime.h> // ktime representation
#include <linux/math64.h> // div_u64
#include <linux/slab.h> // kmalloc
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include "kleb.h"

#include <linux/kprobes.h>

// DEBUG
#include <linux/time.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	#define UNLOCKED 1
#endif

MODULE_LICENSE("GPL"); // Currently using the MIT license
MODULE_AUTHOR("James Bruska");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.4.0");

static struct hrtimer hr_timer;
static ktime_t ktime_period_ns;
static int delay_in_ns, num_events, num_recordings, counter, timer_restart;
static int** hardware_events;
static int Major;

struct cdev *kernel_cdev;

//DEBUG
//static struct timespec ts1, ts2;




#define DEBUG
#ifdef DEBUG
	#define printk_d(...) printk(KERN_INFO "lprof: " __VA_ARGS__)
#else
	#define printk_d(...)
#endif // DEBUG

#define DO_EXIT_NAME "do_exit"
#define COPY_PROCESS_NAME "copy_process"
#define UPROBE_COPY_PROCESS_NAME "uprobe_copy_process"
#define FINISH_TASK_SWITCH_NAME "finish_task_switch"

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
	if ( timer_restart ){
  	printk_d("In jprobes_handle_finish_task_switch() [%d] -> [%d]\n", prev->pid, current->pid);
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
//here we will try three things
  // 1. We will try to use a jprobe on "finish_task_switch" so we can access
  //    the prev task pointer that is passed in. We can use that to compare
  //    with current to see where we are.
  // 2. We will then use a kprobe (should be faster than jprobe) on do_exit
  //    so we can clear the state.
  // 3. We will also use a kprobe on fork or "copy_process" which we will just
  //    assume works fine.
  /*
  do_exit_kp.pre_handler = &kprobes_handle_do_exit;
  do_exit_kp.addr = (kprobe_opcode_t*)kallsyms_lookup_name("do_exit");
  if (do_exit_kp.addr == 0)
  {
    printk(KERN_INFO "Couldn't find 'do_exit'\n");
    return (-EFAULT);
  }
  */
  int ret = register_kprobe(&do_exit_kp);
  if (ret < 0)
  {
    printk(KERN_INFO "Couldn't register 'do_exit' kprobe\n");
    unregister_all();
    return (-EFAULT);
  }

  /*
  copy_process_kp.pre_handler = &kprobes_handle_copy_process;
  copy_process_kp.addr = (kprobe_opcode_t*)kallsyms_lookup_name("copy_process");
  if (copy_process_kp.addr == 0)
  {
    printk(KERN_INFO "Couldn't find 'copy_process'\n");
    return (-EFAULT);
  }
  */
  ret = register_kprobe(&copy_process_kp);
  if (ret < 0)
  {
    //On one of my machines, the name for copy_process in kallsyms was copy_process.
    //  part.27 -- it had a suffix. An alternative would be uprobe_copy_process 
    //  which is called at the end of the copy_process function on success.
    //TODO: consider removing copy_process in the first place.
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

  /*
  finish_task_switch_jp.entry = (kprobe_opcode_t *)&jprobes_handle_finish_task_switch;
  finish_task_switch_jp.kp.addr = kallsyms_lookup_name("finish_task_switch");
  if (finish_task_switch_jp.kp.addr == 0)
  {
    printk(KERN_INFO "Couldn't find 'finish_task_switch'\n");
    return (-EFAULT);
  }
  */
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

	// DEBUG
	//int i;
	//long int old_time_ns = ts2.tv_nsec;
	//getnstimeofday( &ts2 );
	//printk( "delay: %ld\n", ts2.tv_nsec - old_time_ns);	
	//printk( "%ld - \n%ld = \n%ld\n", ts2.tv_nsec, ts1.tv_nsec, ts2.tv_nsec - ts1.tv_nsec);

	if ( timer_restart ) {
		// DEBUG
		printk( "counter: %d\n", counter );
		/*for ( i=0; i < num_events; ++i){
			hardware_events[i][counter] = i;
		}*/

		++counter;
		kt_now = hrtimer_cb_get_time( &hr_timer );
		overrun = hrtimer_forward( &hr_timer, kt_now, ktime_period_ns );
		
		// DEBUG
		//printk( KERN_INFO "overrun: %d ; kt_nsec: %lld \n", overrun, ktime_to_ns( kt_now ) );

		return HRTIMER_RESTART;
	} else {
		printk( "Timer Expired" );
		return HRTIMER_NORESTART;
	}
}

int open( struct inode *inode, struct file *fp ) {
	printk( KERN_INFO "Inside open\n" );
	return 0;
}

ssize_t read( struct file *filep, char *buffer, 
                   size_t len, loff_t *offset ) {
	int size_of_message = num_recordings * num_events;
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

int start_counters() {
	if ( !timer_restart ){
		timer_restart = 1;
		counter = 0;
		ktime_period_ns = ktime_set( 0, delay_in_ns );
		hrtimer_start( &hr_timer, ktime_period_ns, HRTIMER_MODE_REL );
	} else {
		printk( KERN_INFO "Invalid action: Counters already collecting\n" );
	}

	// DEBUG
	//getnstimeofday( &ts1 );
	
	return 0;
}

int stop_counters() {
	// DEBUG
	//int i, j;
	//for ( i=0; i < num_events; ++i ) {
		//for ( j=0; j < counter; ++j ) {
	/*for ( i=0; i < 1; ++i ) {
		for ( j=0; j < 5; ++j ) {
			printk("%d ", hardware_events[i][j]);
		}
		printk("\n");
	}*/

	timer_restart = 0;

	return 0;
}

#ifdef UNLOCKED

long ioctl_funcs( struct file *fp, unsigned int cmd, unsigned long arg ) {
	int ret = 0;

  switch( cmd ) {
		case IOCTL_DEFINE_COUNTERS:
			printk( KERN_INFO "This will define the counters\n" );
			break;
		case IOCTL_START:
			printk( KERN_INFO "Starting counters\n" );
			start_counters();
			break;
		case IOCTL_STOP:
			printk( KERN_INFO "Stopping counters\n" );
			stop_counters();
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

#else

int ioctl_funcs( struct inode *inode, struct file *fp,
                  unsigned int cmd, unsigned long arg ) {
	int data = 10, ret;

	switch( cmd ) {
		case IOCTL_DEFINE_COUNTERS:
			printk( KERN_INFO "This will define the counters\n" );
			break;
		case IOCTL_START:
			printk( KERN_INFO "Starting counters\n" );
			start_counters();
			break;
		case IOCTL_STOP:
			printk( KERN_INFO "Stopping counters\n" );
			stop_counters();
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
	ioctl: ioctl_funcs,
	release: release
};

#endif

int initialize_memory() {
	int i;
	
	printk( "Memory initializing\n" );
	
	delay_in_ns = 1E7L;
	num_events = 4;
	num_recordings = div_u64(1E8L, delay_in_ns); // Holds 100ms worth of data
	
	hardware_events = kmalloc( num_events*sizeof(int *), GFP_ATOMIC );
	hardware_events[0] = kmalloc( num_recordings*sizeof(int), GFP_ATOMIC );
	for ( i=0; i < num_events; ++i ) { // This reduces the number of kmalloc calls
		hardware_events[i] = *hardware_events + num_recordings * i;
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
	printk( "Cleaning up memory\n" );
	
	kfree(hardware_events[0]);
	kfree(hardware_events);

	return 0;
}

int cleanup_timer() {
	int ret;
	
	printk( "Cleaning up timer\n" );

	ret = hrtimer_cancel( &hr_timer );
	if ( ret ) printk( "The timer was still in use...\n" );

	return 0;	
}

int cleanup_ioctl() {
	printk( "Cleaning up IOCTL\n" );

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




