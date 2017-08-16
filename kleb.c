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
#include "kleb.h"

// DEBUG
#include <linux/time.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	#define UNLOCKED 1
#endif

MODULE_LICENSE("GPL"); // Currently using the MIT license
MODULE_AUTHOR("James Bruska");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.3.0");

static struct hrtimer hr_timer;
static ktime_t ktime_period_ns;
static int delay_in_ns, num_events, num_recordings, counter;
static int** hardware_events;
static int Major;

struct cdev *kernel_cdev;

//DEBUG
//static struct timespec ts1, ts2;

enum hrtimer_restart hrtimer_callback( struct hrtimer *timer ) {
	ktime_t kt_now;
	int overrun;	

	// DEBUG
	//int i;
	//long int old_time_ns = ts2.tv_nsec;
	//getnstimeofday( &ts2 );
	//printk( "delay: %ld\n", ts2.tv_nsec - old_time_ns);	
	//printk( "%ld - \n%ld = \n%ld\n", ts2.tv_nsec, ts1.tv_nsec, ts2.tv_nsec - ts1.tv_nsec);

	if ( counter <= 5 ) {
		// DEBUG
		printk( "counter: %d\n", counter );
		/*for ( i=0; i < num_events; ++i){
			hardware_events[i][counter] = i*10 + counter;
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

int release( struct inode *inode, struct file *fp ) {
  printk( KERN_INFO "Inside close\n" );
  return 0;
}

#ifdef UNLOCKED

long ioctl_funcs( struct file *fp, unsigned int cmd, unsigned long arg ) {
  int ret = 0;

  switch( cmd ) {
    case IOCTL_HELLO:
      printk( KERN_INFO "Hello ioctl world" );
      break;
    default:
      printk( KERN_INFO "Invalid command" );
      break;
  }

  return ret;
}

struct file_operations fops = {
  open: open,
  unlocked_ioctl: ioctl_funcs,
  release: release
};

#else

int ioctl_funcs( struct inode *inode, struct file *fp,
                  unsigned int cmd, unsigned long arg ) {
  int data = 10, ret;

  switch( cmd ) {
    case IOCTL_HELLO:
      printk( KERN_INFO "Hello ioctl world" );
      break;
    default:
      printk( KERN_INFO "Invalid command" );
      break;
  }

  return ret;
}

struct file_operations fops = {
  open: open,
  ioctl: ioctl_funcs,
  release: release
};

#endif

int initialize_memory() {
	int i;
	
	printk( "Memory initializing\n" );
	
	delay_in_ns = 1E6L;
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

	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &hrtimer_callback;
	ktime_period_ns = ktime_set( 0, delay_in_ns );
	
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
  if(ret < 0){
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

	printk( "Starting timer to callback in %dns\n", delay_in_ns );

	hrtimer_start( &hr_timer, ktime_period_ns, HRTIMER_MODE_REL );
	
	// DEBUG
	//getnstimeofday( &ts1 );

	printk( "K-LEB module initialized\n" );
	return 0;
}

int cleanup_memory() {
	// DEBUG
	//int i, j;
	
	printk( "Cleaning up memory\n" );

	// DEBUG
	/*for ( i=0; i < num_events; ++i ) {
			for ( j=0; j < counter; ++j ) {
				printk("%d ", hardware_events[i][j]);
			}
			printk("\n");
	}*/

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

	printk( "K-LEB module uninstalled\n" );

	return;
}


