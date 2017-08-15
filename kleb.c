#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h> // high res timer
#include <linux/ktime.h> // ktime representation
#include <linux/math64.h> // div_u64
#include <linux/slab.h> // kmalloc
#include <linux/device.h>
//#include <linux/


// DEBUG
#include <linux/time.h>

MODULE_LICENSE("GPL"); // Currently using the MIT license
MODULE_AUTHOR("James Bruska");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.2.0");

static struct hrtimer hr_timer;
static ktime_t ktime_period_ns;
static int delay_in_ns, num_events, num_recordings, counter;
static int** hardware_events;

//DEBUG
//static struct timespec ts1, ts2;

enum hrtimer_restart hrtimer_callback( struct hrtimer *timer )
{
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

int init_module( void )
{
	int i;

	delay_in_ns = 1E6L;
	num_events = 4;
	num_recordings = div_u64(1E8L, delay_in_ns); // Holds 100ms worth of data
	counter = 0;
	
	hardware_events = kmalloc( num_events*sizeof(int *), GFP_ATOMIC );
	hardware_events[0] = kmalloc( num_recordings*sizeof(int), GFP_ATOMIC );
	for ( i=0; i < num_events; ++i ) { // This reduces the number of kmalloc calls
		hardware_events[i] = *hardware_events + num_recordings * i;
	}

	printk( "HR Timer module initializing\n" );

	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &hrtimer_callback;
	ktime_period_ns = ktime_set( 0, delay_in_ns );

	// DEBUG	
	//int test = hrtimer_is_hres_active( &hr_timer );	
	//printk( "Res is: %d\n",  test);

	printk( "Starting timer to callback in %dus\n", delay_in_ns );

	hrtimer_start( &hr_timer, ktime_period_ns, HRTIMER_MODE_REL );
	
	// DEBUG
	//getnstimeofday( &ts1 );

	printk( "HR Timer module initialized successfully\n" );
	return 0;
}

void cleanup_module( void )
{
	int ret;
	// DEBUG
	//int i, j;

	printk( "HR Timer module uninstalling\n" );
	
	ret = hrtimer_cancel( &hr_timer );
	if ( ret ) printk( "The timer was still in use...\n" );

	// DEBUG
	/*for ( i=0; i < num_events; ++i ) {
			for ( j=0; j < counter; ++j ) {
				printk("%d ", hardware_events[i][j]);
			}
			printk("\n");
	}*/

	kfree(hardware_events[0]);
	kfree(hardware_events);	

	printk( "HR Timer module uninstalled\n" );

	return;
}


