#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>
//#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Bruska");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.1.1");

//#define US_TO_NS(x) (x * 1E3L)

static struct hrtimer hr_timer;
static int delay_in_ns, num_events, num_recordings, counter;
static int** hardware_events;

//DEBUG
//static struct timespec ts1, ts2;

enum hrtimer_restart hrtimer_callback( struct hrtimer *timer )
{
	int i;
	// DEBUG
	//getnstimeofday( &ts2 );
	//printk( "%ld - \n%ld = \n%ld\n", ts2.tv_nsec, ts1.tv_nsec, ts2.tv_nsec - ts1.tv_nsec);

	if ( counter <= 5 ) {
		// DEBUG
		printk( "counter: %d; ", counter );
		/*for ( i=0; i < num_events; ++i){
			hardware_events[i][counter] = i*10 + counter;
		}*/

		++counter;
		return HRTIMER_RESTART;
	} else {
		printk( "Timer Expired" );
		return HRTIMER_NORESTART;
	}
}

int init_module( void )
{
	int i;
	ktime_t ktime;

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

	ktime = ktime_set( 0, delay_in_ns );
	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &hrtimer_callback;

	// DEBUG	
	//int test = hrtimer_is_hres_active( &hr_timer );	
	//printk( "Res is: %d\n",  test);

	printk( "Starting timer to callback in %dus\n", delay_in_ns );

	hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
	
	// DEBUG
	//getnstimeofday( &ts1 );

	printk( "HR Timer module initialized successfully\n" );
	return 0;
}

void cleanup_module( void )
{
	int ret, i, j;

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


