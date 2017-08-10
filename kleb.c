#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/delay.h>

MODULE_LICENSE("MIT");
MODULE_AUTHOR("James Bruska");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.1");

#define US_TO_NS(x) (x * 1E3L)

static struct hrtimer hr_timer;

//DEBUG
static struct timespec ts1, ts2;

enum hrtimer_restart hrtimer_callback( struct hrtimer *timer )
{
	// DEBUG
	getnstimeofday( &ts2 );
	printk( "%ld - \n%ld = \n%ld\n", ts2.tv_nsec, ts1.tv_nsec, ts2.tv_nsec - ts1.tv_nsec);

	printk( "Timer Expired" );

	return HRTIMER_NORESTART;
}

int init_module( void )
{
	ktime_t ktime;
	unsigned long delay_in_us = 1000L;
	
	printk( "HR Timer module initializing\n" );

	ktime = ktime_set( 0, US_TO_NS(delay_in_us) );
	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &hrtimer_callback;

	// DEBUG	
	//int test = hrtimer_is_hres_active( &hr_timer );	
	//printk( "Res is: %d\n",  test);

	printk( "Starting timer to callback in %ldus\n", delay_in_us );

	hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
	
	// DEBUG
	getnstimeofday( &ts1 );

	printk( "HR Timer module initialized successfully\n" );
	return 0;
}

void cleanup_module( void )
{
	int ret;

	ret = hrtimer_cancel( &hr_timer );
	if ( ret ) printk( "The timer was still in use...\n" );

	printk( "HR Timer module uninstalling\n" );

	return;
}


