/* Copyright (c) 2017, 2023 James Bruska, Caleb DeLaBruere, Chutitep Woralert

This file is part of K-LEB.

K-LEB is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

K-LEB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with K-LEB.  If not, see <https://www.gnu.org/licenses/>. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>	// high res timer
#include <linux/ktime.h>	// ktime representation
#include <linux/math64.h>	// div_u64
#include <linux/slab.h>		// kmalloc
#include <linux/device.h>	// character devices
#include <linux/fs.h>		// file control
#include <linux/cdev.h>
#include <linux/version.h>	// linux version
#include <asm/uaccess.h>
#include <asm/perf_event.h>	// union cpuid10...
#include "kleb.h"

#include <linux/kprobes.h> 	// kprobe and jprobe
#include <linux/sched.h> 	//finish_task_switch

#include <linux/time.h>
#include <linux/delay.h>


#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36)
#define UNLOCKED 1
#endif
unsigned long __stack_chk_guard;
void __stack_chk_guard_setup(void)
{
     __stack_chk_guard = 0xBAAAAAAD;//provide some magic numbers
}
MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Bruska, Caleb DeLaBruere, Chutitep Woralert");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.8.0");

/* Module parameters */
static struct hrtimer hr_timer;
static ktime_t ktime_period_ns;
static unsigned int delay_in_ns;
static int num_events, num_recordings, counter, timer_restart, num_cores;
static int target_pid, recording, memcheck;
static unsigned int **hardware_events;
static int Major;
static kleb_ioctl_args_t kleb_ioctl_args;
static int sysmode;
/* For tapping */
struct cdev *kernel_cdev;

/* Counters parameters */
static int event_on, event_off, event_reset;
static long eventname;
long int count_in;

/* Handle context switch & CPU switch */
static int start_init;
static int fork_check;
static int first_hpc;
static int target_ppid;
static int atarget_pid[100];
static int atarget_ppid[100];
static int acpucheck[100];
static int targetloop, targetloopf;
static int newfork, newthread, activepid;
/* AMD event selection */
uint32_t evtcount[7];

/* Timer core */
static struct hrtimer hr_timer_core[16];
static unsigned int hardware_events_core[7];
static int timer_restart_core[16];

/* Initialize counters */
static long pmu_start_counters(unsigned int counter1, unsigned int counter2, unsigned int counter3, unsigned int counter4, unsigned long long counter_umask, unsigned int user_os_rec)
{
	// ******** SET CONFIGURABLE COUNTERS ********//
	memset(hardware_events_core, 0, sizeof(hardware_events_core));
	memset(evtcount, 0, sizeof(evtcount));
	if( num_events == 6 ){
		event_on = ( 1 << 31 ) | ( 1 << 5)| ( 1 << 4)| ( 1 << 3)| ( 1 << 2)| ( 1 << 1)| ( 1 << 0);
	}	
	else if ( num_events == 5 ){
		event_on = ( 1 << 31 ) | ( 0 << 5)| ( 1 << 4)| ( 1 << 3)| ( 1 << 2)| ( 1 << 1)| ( 1 << 0);
	}
	else if ( num_events == 4 ){
		event_on = ( 1 << 31 ) | ( 0 << 5)| ( 0 << 4)| ( 1 << 3)| ( 1 << 2)| ( 1 << 1)| ( 1 << 0);
	}
	else if ( num_events == 3 ){
		event_on = ( 1 << 31 ) | ( 0 << 5)| ( 0 << 4)| ( 0 << 3)| ( 1 << 2)| ( 1 << 1)| ( 1 << 0);
	}
	else if ( num_events == 2 ){
		event_on = ( 1 << 31 ) | ( 0 << 5)| ( 0 << 4)| ( 0 << 3)| ( 0 << 2)| ( 1 << 1)| ( 1 << 0);
	}
	else{
		event_on = ( 1 << 31 ) | ( 0 << 5)| ( 0 << 4)| ( 0 << 3)| (01 << 2)| ( 0 << 1)| ( 1 << 0);
	}
	event_off = ( 0 << 31 ) | ( 0 << 5)| ( 0 << 4)| ( 0 << 3)| ( 0 << 2)| ( 0 << 1)| ( 0 << 0);
	event_reset = ( 0 << 5 ) | (0 << 3) | (1 << 2) | (1 << 1) | (1 << 0) ;
		
	return 0;
}

/* Disable counting */
static long pmu_stop_counters(void)
{
	/* Disable configurable counters */
	asm volatile("msr   PMCNTENSET_EL0, %0" : : "r"(0));
	return 0;
}

/* Enable counting */
static long pmu_restart_counters(void)
{
	/* Enable counters on global counter control  */
	
	//--Enable control register && reset counter
	asm volatile("msr   PMCR_EL0, %0" : : "r"(event_reset));

	if( num_events >= 1 ){
	eventname = ( 1 << 31 ) | kleb_ioctl_args.counter[0];
	asm volatile("msr   PMEVTYPER0_EL0, %0" : : "r"(eventname));
	}
	if( num_events >= 2 ){
	eventname = ( 1 << 31 ) | kleb_ioctl_args.counter[1];
	asm volatile("msr   PMEVTYPER1_EL0, %0" : : "r"(eventname));
	}
	if( num_events >= 3 ){
	eventname = ( 1 << 31 ) | kleb_ioctl_args.counter[2];
	asm volatile("msr   PMEVTYPER2_EL0, %0" : : "r"(eventname));
	}
	if( num_events >= 4 ){
	eventname = ( 1 << 31 ) | kleb_ioctl_args.counter[3];
	asm volatile("msr   PMEVTYPER3_EL0, %0" : : "r"(eventname));
	}
	if( num_events >= 5 ){
	eventname = ( 1 << 31 ) | kleb_ioctl_args.counter[4];
	asm volatile("msr   PMEVTYPER4_EL0, %0" : : "r"(eventname));
	}
	if( num_events >= 6 ){
	eventname = ( 1 << 31 ) | kleb_ioctl_args.counter[5];
	asm volatile("msr   PMEVTYPER5_EL0, %0" : : "r"(eventname));
	}

	//--Set global counter           
    asm volatile("msr   PMCNTENSET_EL0, %0" : : "r"(event_on));
    
	return 0;
}

/* Aggregate counters value */
static long pmu_read_counters(void)
{

	int i = 0;
	
	/* Read configuration counters */
	for(i = 0; i < num_events+1; ++i){
		hardware_events_core[i] = evtcount[i];
		hardware_events[i][counter] =  hardware_events_core[i];
	}
	memset(evtcount, 0, sizeof(evtcount));

	return 0;
}

/* Read value from configuration counters */
static long pmu_read_counters_core(void)
{
	uint32_t count;

	asm volatile("mrs  %0, PMCCNTR_EL0" : "=r"(count));
	evtcount[0] += count;

	if( num_events >= 1 ){
	asm volatile("mrs  %0, PMEVCNTR0_EL0" : "=r"(count));
	evtcount[1] += count;
}
	if( num_events >= 2 ){
	asm volatile("mrs  %0, PMEVCNTR1_EL0" : "=r"(count));
	evtcount[2] += count;
}
	if( num_events >= 3 ){
	asm volatile("mrs  %0, PMEVCNTR2_EL0" : "=r"(count));
	evtcount[3] += count;
}
	if( num_events >= 4 ){
	asm volatile("mrs  %0, PMEVCNTR3_EL0" : "=r"(count));
	evtcount[4] += count;
}
	if( num_events >= 5 ){
	asm volatile("mrs  %0, PMEVCNTR4_EL0" : "=r"(count));
	evtcount[5] += count;
}
	if( num_events >= 6 ){
	asm volatile("mrs  %0, PMEVCNTR5_EL0" : "=r"(count));
	evtcount[6] += count;
}
	asm volatile("msr   PMCR_EL0, %0" : : "r"(event_reset));

	return 0;
}

int kprobes_handle_finish_task_switch_pre(struct kprobe *p, struct pt_regs *regs)
{
	if(recording && (counter < num_recordings) && start_init)
	{
		if(sysmode){
			/* System monitoring tracker */
			if(!timer_restart_core[current->cpu] && current->pid!=0 && current->pid!=1){
				timer_restart_core[current->cpu] = 1;
				pmu_restart_counters();
				/* Initialize main hrtimer */
				if (counter < num_recordings && first_hpc == 0)
				{
					ktime_period_ns = ktime_set(0, delay_in_ns);
					hrtimer_start(&hr_timer, ktime_period_ns, HRTIMER_MODE_REL);
					//printk(KERN_INFO "Timer start on PID: %d CPU: %d GCPU: %d", current->pid, current->cpu, get_cpu());
					//printk(KERN_INFO "Main Timer start on CPU: %d", current->cpu);		
				}
				++first_hpc;	
				/* Initialize sub hrtimers */
				hrtimer_start(&hr_timer_core[current->cpu], ktime_period_ns, HRTIMER_MODE_REL);	
			}
		}
		else{
			/* Process Monitoring */
			for(targetloop = 0; targetloop <= fork_check; ++targetloop){
				if (current->pid == atarget_pid[targetloop])
				{
					//printk(KERN_INFO "Process in: %s [%d] [%d] #Fork: %d Index: %d\n", current->comm, current->pid, current->cpu, fork_check, targetloop);
					//printk(KERN_INFO "Process in: pid[%d] tgid[%d]",current->pid,current->tgid);
					timer_restart_core[current->cpu] = 1;
					pmu_restart_counters();
					/* Initialize main hrtimer */
					if (counter < num_recordings && first_hpc == 0)
					{
						ktime_period_ns = ktime_set(0, delay_in_ns);
						hrtimer_start(&hr_timer, ktime_period_ns, HRTIMER_MODE_REL);
						//printk(KERN_INFO "Main Timer start on CPU: %d", current->cpu);
						//printk(KERN_INFO "Timer start on PID: %d CPU: %d GCPU: %d", current->pid, current->cpu, get_cpu());
					}
					++first_hpc;
					/* Initialize sub hrtimers */
					ktime_period_ns = ktime_set(0, delay_in_ns);
					hrtimer_start(&hr_timer_core[current->cpu], ktime_period_ns, HRTIMER_MODE_REL);
					atarget_ppid[targetloop] = 1;
					acpucheck[targetloop] = current->cpu;	
					break;
				}
				else if (current->parent->pid == atarget_pid[targetloop])
				{
					/* Fork detected */
					newfork = 1;
					/* Check if fork exist */
					for(targetloopf = 0; targetloopf <= fork_check; ++targetloopf){
						if(atarget_pid[targetloopf] == current->pid){
							newfork = 0;
							break;
						}
					}
					/* Add new fork pid to tracker */
					if(newfork){
						++fork_check;
						++activepid;	
						atarget_pid[fork_check] = current->pid;
						//printk(KERN_INFO "Fork detect!!: %s PID: %d CPU: %d", current->comm, current->pid, current->cpu);
					}
					
				}
				else if (current->tgid == atarget_pid[targetloop] && current->tgid != 0){
					/* Thread detected */
					newthread = 1;
					/* Check if thread exist */
					for(targetloopf = 0; targetloopf <= fork_check; ++targetloopf){
						if(atarget_pid[targetloopf] == current->pid){
							newthread = 0;
							break;
						}
					}
					/* Add new thread to tracker */
					if(newthread){
						//printk(KERN_INFO "Thread detect!!: %s pid: %d tgid: %d cpu: %d old cpu: %d", current->comm, current->pid, current->tgid, current->cpu, acpucheck[targetloop]);
						//printk(KERN_INFO "Process (fork) in: %s [%d] [%d] [%d]\n", current->comm, current->pid, current->cpu, current->state);
						++fork_check;
						++activepid;	
						atarget_pid[fork_check] = current->pid;
						acpucheck[fork_check] = current->cpu;
					}
				}		
				if (current->pid != atarget_pid[targetloop] && atarget_ppid[targetloop] && current->cpu == acpucheck[targetloop])
				{
					//printk(KERN_INFO "Process out: %s [%d] [%d] #Fork: %d Index: %d\n", current->comm, current->pid, current->cpu, fork_check, targetloop);
					//printk(KERN_INFO "Process out: pid[%d] tgid[%d]",current->pid,current->tgid);
					/* Process switch out of cpu */
					atarget_ppid[targetloop] = 0;	
							
					/* Stop counter and extract value */
					hrtimer_cancel(&hr_timer_core[current->cpu]);						
					pmu_stop_counters();			
					timer_restart_core[current->cpu] = 0;
					if (counter < num_recordings)
					{	
						pmu_read_counters_core();
					}
					//--first_hpc;
				}
				
			}
			
		}
	}

	return 0;
}
/*void kprobes_handle_finish_task_switch_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	if(recording && (counter < num_recordings) && start_init)
	{
			if (current->pid == target_pid)
			{
				//printk(KERN_INFO "finish_task_switch_post: %s [%ld] [%d] [%d]\n", current->comm, current->pid, current->cpu, current->state);
			}
		
	}
}*/
/*int kprobes_handle_prepare_task_switch_pre(struct kprobe *p, struct pt_regs *regs)
{
	if(recording && (counter < num_recordings) && start_init)
	{
			if (current->pid == target_pid)
			{
				printk(KERN_INFO "prepare_task_switch_pre: %s [%d] [%d] [%d]\n", current->comm, current->pid, current->cpu, current->state);
			}
		
	}
	return 0;
}
void kprobes_handle_prepare_task_switch_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	if(recording && (counter < num_recordings) && start_init)
	{
			if (current->pid == target_pid)
			{
				printk(KERN_INFO "prepare_task_switch_post: %s [%d] [%d] [%d]\n", current->comm, current->pid, current->cpu, current->state);
			}
		
	}
}*/
static int kprobes_handle_do_exit_pre(struct kprobe *p, struct pt_regs *regs)
{
	if(recording && (counter < num_recordings) && start_init && !sysmode)
	{
		/* Detect process exit */
		for(targetloop = 0; targetloop <= fork_check; ++targetloop){
			if (current->pid == atarget_pid[targetloop])
			{	 
				/* Stop timer & extract counter values */
				--activepid;
				pmu_stop_counters();
				timer_restart_core[current->cpu] = 0;
				atarget_ppid[targetloop] = 0;
				if (counter < num_recordings)
				{
					pmu_read_counters_core();
				}
				/* All active process exit stop monitoring */
				if(activepid <= 0){
					int i;
					hrtimer_cancel(&hr_timer);	 
					for(i = 0; i < num_cores; i++){
						hrtimer_cancel(&hr_timer_core[i]);	
					} 
				}
				//printk(KERN_INFO "do_exit_pre: %s [%d] [%d] [%ld] #pid: %d\n", current->comm, current->pid, current->cpu, current->state, activepid);
			}
		}
	}
	return 0;
}
static struct kprobe finish_task_switch_kp = {
	.pre_handler = kprobes_handle_finish_task_switch_pre,
	//.post_handler = kprobes_handle_finish_task_switch_post,
	.symbol_name = DO_EXIT_NAME,
};

 static struct kprobe do_exit_kp = {
	 .pre_handler = kprobes_handle_do_exit_pre,
			//.post_handler = kprobes_handle_do_exit_post,
	.symbol_name = "do_exit",
};
/*static struct kprobe prepare_task_switch_kp = {
	.pre_handler = kprobes_handle_prepare_task_switch_pre,
	.post_handler = kprobes_handle_prepare_task_switch_post,
	.symbol_name = DO_ENTER_NAME,
};*/

void unregister_all(void)
{
	unregister_kprobe(&finish_task_switch_kp);
	unregister_kprobe(&do_exit_kp);
}

int register_all(void)
{
	/* Register probes */
	int ret = register_kprobe(&finish_task_switch_kp);
	if (ret < 0)
	{
		printk(KERN_INFO "Couldn't register 'finish_task_switch' kprobe %d\n", ret);
		unregister_all();
		return (-EFAULT);
	}
		 ret = register_kprobe(&do_exit_kp);
		 	if (ret < 0)
					{
								printk(KERN_INFO "Couldn't register 'do_exit' kprobe %d\n", ret);
										unregister_all();
												return (-EFAULT);
											}
	/* ret = register_kprobe(&prepare_task_switch_kp);
	if (ret < 0)
	{
		printk(KERN_INFO "Couldn't register 'prepare_task_switch' kprobe %d\n", ret);
		unregister_all();
		return (-EFAULT);
	}*/

	return (0);
}

/* Restart timer */
enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
	ktime_t kt_now;

	/* Restart timer */
	if (recording && timer_restart && (counter < num_recordings) && first_hpc > 0)
	{
		/* Read counter */
		pmu_read_counters();
		++counter;
		
		/* Forward timer */
		kt_now = hrtimer_cb_get_time(&hr_timer);
		hrtimer_forward(&hr_timer, kt_now, ktime_period_ns);

		return HRTIMER_RESTART;
	}
	/* Overflow restart timer */
	else
	{
		if (!timer_restart)
		{
			printk("Main Timer Expired on CPU %d\n", current->cpu);
		}
		else if (counter > num_recordings)
		{
			printk("Counter > allowed spaces: %d > %d\n", counter, num_recordings);
		}
		return HRTIMER_NORESTART;
	}
}

enum hrtimer_restart hrtimer_callback_core(struct hrtimer *timer)
{
	ktime_t kt_now;

	/* Restart timer */
	if (timer_restart_core[current->cpu] && (counter < num_recordings))
	{
		/* Read counter */
		pmu_read_counters_core();
		hrtimer_forward_now(&hr_timer_core[current->cpu], ktime_period_ns);	
		return HRTIMER_RESTART;
	}
	/* Overflow restart timer */
	else
	{
		if (!timer_restart_core[current->cpu])
		{
			printk("Timer on CPU:%d Expired\n", current->cpu);
		}
		else if (counter > num_recordings)
		{
			printk("Counter > allowed spaces: %d > %d\n", counter, num_recordings);
		}
		return HRTIMER_NORESTART;
	}
}
/* Initialize module from ioctl start */
int start_counters( unsigned int pmu_counter1, unsigned int pmu_counter2, unsigned int pmu_counter3, unsigned int pmu_counter4, unsigned long long pmu_counter_umask, unsigned int pmu_user_os_rec)
{
	int i, j;

	if (!recording)
	{
		recording = 1;
		timer_restart = 1;
		counter = 0;
		/* Set empty buffer */
		for (i = 0; i < (num_events + 1); ++i)
		{ 
			for (j = 0; j < num_recordings; ++j)
			{
				hardware_events[i][j] = -10;
			}
		}
		/* Initialize counters */
		pmu_start_counters(pmu_counter1, pmu_counter2, pmu_counter3, pmu_counter4, pmu_counter_umask, pmu_user_os_rec);
		
		fork_check=0;
		activepid=1;
		start_init=1;
		first_hpc=0;
		
		target_ppid = 1;
		
		memset(atarget_pid, 0, sizeof(atarget_pid));
		memset(atarget_ppid, 0, sizeof(atarget_ppid));
		memset(acpucheck, 0, sizeof(acpucheck));
		memset(timer_restart_core, 0, sizeof(timer_restart_core));

		atarget_pid[0] = target_pid;
	}
	else
	{
		printk(KERN_INFO "Invalid action: Counters already collecting\n");
	}

	return 0;
}

/* Deinitialize module from ioctl stop */
int stop_counters()
{
	/* Stop counters */
	int i;
	hrtimer_cancel(&hr_timer);
	for(i = 0; i < num_cores; i++){
		hrtimer_cancel(&hr_timer_core[i]);
	}
	pmu_stop_counters();
	pmu_read_counters();
	counter++;
	recording = 0;
	timer_restart = 0;
	fork_check = 0;
	activepid=0;
	first_hpc = 0;
	start_init=0;
	target_ppid = 1;
	memset(atarget_pid, 0, sizeof(atarget_pid));
	memset(atarget_ppid, 0, sizeof(atarget_ppid));
	memset(acpucheck, 0, sizeof(acpucheck));
	memset(hardware_events_core, 0, sizeof(hardware_events_core));
	memset(timer_restart_core, 0, sizeof(timer_restart_core));
	return 0;
}

int open(struct inode *inode, struct file *fp)
{
	printk(KERN_INFO "Inside open\n");
	return 0;
}

/* Read for extract data to user */
ssize_t read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int size_of_message = num_recordings * (num_events + 1) * sizeof(unsigned int);
	int size_of_realmessage = counter * (num_events + 1) * sizeof(unsigned int);
	int error_count = 0;
	int x, y;
	/* For periodic data extraction */
	if(recording){
		//FOR DEBUG
		//printk("Counter INT: %d On CPU: %d\n", counter, get_cpu());
		
		/* Read and save latest HPC */
		if(counter != 0){		
			/* Send data to user */
			error_count = copy_to_user(buffer, hardware_events[0], size_of_message);
			counter = 0;

			/* Reset buffer value */
			for (x = 0; x < (num_events + 1); ++x)
			{ 
				for (y = 0; y < num_recordings; ++y)
				{
					hardware_events[x][y] = -10;
				}
			}
		}
	}
	/* For exit data extraction */
	else if(!recording && memcheck)	
	{
		error_count = copy_to_user(buffer, hardware_events[0], size_of_message);
		if (cleanup_memory() < 0)
		{
			printk(KERN_INFO "Memory failed to cleanup cleanly");
		}
	}
	else
	{
		printk(KERN_INFO "There is no data left to extract");
	}
	/* Check data extraction size */
	if (error_count == 0 && size_of_realmessage != 0)
	{
		//DEBUG
		//printk(KERN_INFO "Sent %d characters to the user\n", size_of_realmessage);
		return (size_of_realmessage);
	}
	else if(error_count == 0 && size_of_realmessage == 0){
		return (size_of_realmessage);
	}
	else
	{
		printk(KERN_INFO "Failed to send %d characters to the user\n", error_count);
		return -EFAULT;
	}
	
}

int release(struct inode *inode, struct file *fp)
{
	printk(KERN_INFO "Inside close\n");
	return 0;
}

#ifdef UNLOCKED
long ioctl_funcs(struct file *fp, unsigned int cmd, unsigned long arg)
#else
int ioctl_funcs(struct inode *inode, struct file *fp, unsigned int cmd, unsigned long arg)
#endif
{
	int ret = 0;
	kleb_ioctl_args_t *kleb_ioctl_args_user = (kleb_ioctl_args_t *)(arg);
	if (kleb_ioctl_args_user == NULL)
	{
		printk_d("lprof_ioctl: User did not pass in cmd\n");
		return (-EINVAL);
	}
	else{
		//DEBUG
		//printk(KERN_INFO "************ Catch IOCTL ***********\n");
		//printk(KERN_INFO "%u\n", cmd);
	}

	/* Read the parameters from userspace */
	if (copy_from_user(&kleb_ioctl_args, kleb_ioctl_args_user, sizeof(kleb_ioctl_args_t)) != 0)
	{
		printk_d("lprof_ioctl: Could not copy cmd from userspace\n");
		return (-EINVAL);
	}
	switch (cmd)
	{
		case IOCTL_DEFINE_COUNTERS:
			printk(KERN_INFO "This will define the counters\n");
			break;
		/* Start command */
		case IOCTL_START:
			printk(KERN_INFO "Starting counters\n");
			target_pid = kleb_ioctl_args.pid;
			delay_in_ns = kleb_ioctl_args.delay_in_ns;
			num_events = kleb_ioctl_args.num_events;
			if (initialize_memory() < 0)
			{
				printk(KERN_INFO "Memory failed to initialize");
				return (-ENODEV);
			}
			printk(KERN_INFO "target pid: %d\n", (int)target_pid);
			if(target_pid == 1 || target_pid == 0){
					sysmode = 1;
			}
			else{
					sysmode = 0;
			}
			//DEBUG
			//printk(KERN_INFO "%d %d %d %d %llu %d\n",kleb_ioctl_args.counter1, kleb_ioctl_args.counter2, kleb_ioctl_args.counter3,kleb_ioctl_args.counter4, kleb_ioctl_args.counter_umask, kleb_ioctl_args.user_os_rec);
			//printk(KERN_INFO "%lu %lu %lu %lu %lu %lu %d\n",kleb_ioctl_args.counter[0], kleb_ioctl_args.counter[1], kleb_ioctl_args.counter[2],kleb_ioctl_args.counter[3], kleb_ioctl_args.counter[4], kleb_ioctl_args.counter[5], kleb_ioctl_args.num_events);
			start_counters(kleb_ioctl_args.counter[1], kleb_ioctl_args.counter[2], kleb_ioctl_args.counter[3],kleb_ioctl_args.counter[4], kleb_ioctl_args.counter[5], kleb_ioctl_args.user_os_rec);
			break;
		case IOCTL_DUMP:
			printk(KERN_INFO "This will dump the counters\n");
		break;
		/* Stop command */
		case IOCTL_STOP:
			printk(KERN_INFO "Stopping counters\n");
			stop_counters();
			break;
		case IOCTL_DELETE_COUNTERS:
			printk(KERN_INFO "This will delete the counters\n");
			break;
		case IOCTL_DEBUG:
			printk(KERN_INFO "This will set up debug mode\n");
			break;
		case IOCTL_STATS:
			printk(KERN_INFO "This will set up profiling mode\n");
			break;
		default:
			printk(KERN_INFO "Invalid command\n");
			break;
	}

	return ret;
}

#ifdef UNLOCKED
struct file_operations fops = {
	open : open,
	read : read,
	unlocked_ioctl : ioctl_funcs,
	release : release
};
#else
struct file_operations fops = {
	open : open,
	read : read,
	ioctl : ioctl_funcs,
	release : release
};
#endif

int initialize_memory()
{
	int i, j;

	printk("Memory initializing\n");

	num_recordings = 500;
	/* Create data buffer */
	hardware_events = kmalloc((num_events + 1) * sizeof(unsigned int *), GFP_KERNEL);
	hardware_events[0] = kmalloc((num_events + 1) * num_recordings * sizeof(unsigned int), GFP_KERNEL);
	for (i = 0; i < (num_events + 1); ++i)
	{ // This reduces the number of kmalloc calls
		hardware_events[i] = *hardware_events + num_recordings * i;
		for (j = 0; j < num_recordings; ++j)
		{
			hardware_events[i][j] = -10;
		}
	}
	memcheck = 1;
	return 0;
}

int initialize_timer()
{
	int i;
	printk("Timer initializing\n");

	counter = 0;
	timer_restart = 0;

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &hrtimer_callback;

	for(i = 0; i < num_cores; i++){
		hrtimer_init(&hr_timer_core[i], CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hr_timer_core[i].function = &hrtimer_callback_core;
	}
	printk("Complete Timer initializing\n");
	return 0;
}

int initialize_ioctl()
{
	int ret;
	dev_t dev_no, dev;

	printk("IOCTL initializing\n");

	kernel_cdev = cdev_alloc();
	kernel_cdev->ops = &fops;
	kernel_cdev->owner = THIS_MODULE;

	ret = alloc_chrdev_region(&dev_no, 0, 1, "char_arr_dev");
	if (ret < 0)
	{
		printk("Major number allocation has failed\n");
		return ret;
	}

	Major = MAJOR(dev_no);
	dev = MKDEV(Major, 0);
	printk("The major number for your device is %d\n", Major);

	ret = cdev_add(kernel_cdev, dev, 1);
	if (ret < 0)
	{
		printk(KERN_INFO "Unable to allocate cdev");
		return ret;
	}

	return 0;
}

int init_module(void)
{
	int ret;

	/*if (initialize_memory() < 0)
	{
		printk(KERN_INFO "Memory failed to initialize");
		return (-ENODEV);
	}*/
	printk(KERN_INFO "This system has %d processors configured and %d processors available.\n", NR_CPUS, nr_cpu_ids);
	num_cores = nr_cpu_ids;
	if (initialize_timer() < 0)
	{
		printk(KERN_INFO "Timer failed to initialize");
		return (-ENODEV);
	}

	if (initialize_ioctl() < 0)
	{
		printk(KERN_INFO "IOCTL failed to initialize");
		return (-ENODEV);
	}

	ret = register_all();
	if (ret != 0)
	{
		return (ret);
	}

	printk("K-LEB module initialized\n");
	return 0;
}

int cleanup_memory()
{
	printk("Memory cleaning up\n");

	kfree(hardware_events[0]);
	kfree(hardware_events);

	memcheck = 0;
	return 0;
}

int cleanup_timer()
{
	int ret;
	int i;
	printk("Timer cleaning up\n");
	
	for(i = 0; i < num_cores; i++){
		hrtimer_cancel(&hr_timer_core[i]);
	}
	ret = hrtimer_cancel(&hr_timer);
	if (ret)
		printk("The timer was still in use...\n");
	return 0;
}

int cleanup_ioctl()
{
	printk("IOCTL cleaning up\n");

	cdev_del(kernel_cdev);
	unregister_chrdev_region(Major, 1);

	return 0;
}

void cleanup_module(void)
{
	/*if (cleanup_memory() < 0)
	{
		printk(KERN_INFO "Memory failed to cleanup cleanly");
	}*/

	if (cleanup_timer() < 0)
	{
		printk(KERN_INFO "Timer failed to cleanup cleanly");
	}

	if (cleanup_ioctl() < 0)
	{
		printk(KERN_INFO "IOCTL failed to cleanupcleanly");
	}

	unregister_all();

	printk("K-LEB module uninstalled\n");

	return;
}
