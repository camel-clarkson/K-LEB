/* Copyright (c) 2017, 2020 Chutitep Woralert, James Bruska, Caleb DeLaBruere, Chen Liu

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
#include <asm/nmi.h>		// reserve_perfctr_nmi ...
#include <asm/perf_event.h>	// union cpuid10...
#include <asm/special_insns.h> // read and write cr4
#include "kleb.h"

#include <linux/kprobes.h> 	// kprobe and jprobe
#include <linux/sched.h> 	//finish_task_switch

#include <linux/time.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36)
#define UNLOCKED 1
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Bruska, Caleb DeLaBruere, Chutitep Woralert");
MODULE_DESCRIPTION("K-LEB: A hardware event recording system with a high resolution timer");
MODULE_VERSION("0.7.0");

/* Module parameters */
static struct hrtimer hr_timer;
static ktime_t ktime_period_ns;
static unsigned int delay_in_ns;
static int num_events, num_recordings, counter, timer_restart;
static int target_pid, recording;
static unsigned int **hardware_events;
static int Major;
static kleb_ioctl_args_t kleb_ioctl_args;
/* For tapping */
struct cdev *kernel_cdev;

/* Counters parameters */
static int reg_addr, reg_addr_val, reg_fixed_addr_val, event_num, umask, enable_bits, disable_bits, event_on, event_off;
static int test_counters[4];
static int addr[4];
static int addr_fixed;
static int addr_global;
static int addr_val[4];
static int addr_fixed_val[3];
static long int eax_low, edx_high;
long int count_in;

/* Handle context switch & CPU switch */
static int start_init, mul_exit;
static unsigned int temp_hpc[9];
static int fork_check, pc_init;
static int first_hpc;

/* Initialize counters */
static long pmu_start_counters(unsigned int counter1, unsigned int counter2, unsigned int counter3, unsigned int counter4, unsigned long long counter_umask, unsigned int user_os_rec)
{
	int i = 0;

	/* Assign IA32_FIXED_CTR_CTRL MSR & MSR_PERF_GLOBAL_CTRL MSR */
	addr_fixed = 0x38d;
	addr_global = 0x38f;

	/* Setup configurable counters */
	/* Assign perfeventsel0-3 */
	addr[0] = 0x186; // IA32_PERFEVTSELx MSRs
	addr[1] = 0x187;
	addr[2] = 0x188;
	addr[3] = 0x189;

	//Assign perfctr0-3
	addr_val[0] = 0xc1; // IA32_PMC MSR // 0xc1 is perfctr0
	addr_val[1] = 0xc2;
	addr_val[2] = 0xc3;
	addr_val[3] = 0xc4;
	
	/* Assign events */
	test_counters[0] = counter1;
	test_counters[1] = counter2;
	test_counters[2] = counter3;
	test_counters[3] = counter4; 
	
	printk_d(KERN_INFO "Events: %d %d %d %d\n", counter1, counter2, counter3, counter4);

	/* Define IA32_PERFEVTSELx MSRs parameters */
	user_os_rec &= 0x01; // Enforces requirement of 0 <= user_os_rec <= 3
	enable_bits = 0x400000 + (user_os_rec << 16);
	disable_bits = 0x100000 + (user_os_rec << 16);
	counter_umask &= 0xFF; // Enforces requirement of 0 <= counter_umask <= 0xFF
	umask = counter_umask << 8;
	eax_low = 0x00;

	/* Setup fixed counters */
	/* Assign fixedperfctr0-2 */
	addr_fixed_val[0] = 0x309; //IA32_FIXED_CTR; // 0xc1 is fixedperfctr0
	addr_fixed_val[1] = 0x30a;
	addr_fixed_val[2] = 0x30b;


	__asm__("wrmsr"
			:
			: "c"(addr_global), "a"(0x00), "d"(0x00));
	
	// ******** SET CONFIGURABLE COUNTERS ********//
	for (i = 0; i < 4; i++)
	{
		reg_addr_val = addr_val[i];
		reg_addr = addr[i];

		__asm__("wrmsr"
				:
				: "c"(reg_addr), "a"(event_off), "d"(0x00));

		__asm__("wrmsr"
				:
				: "c"(reg_addr_val), "a"(0x00), "d"(0x00));
		//Call read to check initial value for debugging
		//__asm__("rdmsr"
		//		: "=a"(eax_low), "=d"(edx_high)
		//		: "c"(reg_addr_val));
		//count_in = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
		//printk( KERN_INFO "rdmsr in:   %ld\n", count_in);
	}

	// ******** SET FIXED COUNTERS ********//
	__asm__("wrmsr"
			:
			: "c"(addr_fixed), "a"(0x000), "d"(0x00));
	
	for (i = 0; i < 3; i++)
	{
			reg_fixed_addr_val = addr_fixed_val[i];

		__asm__("wrmsr"
				:
				: "c"(reg_fixed_addr_val), "a"(0x00), "d"(0x00));
		//Call read to check initial value for debugging
		//__asm__("rdmsr"
		//		: "=a"(eax_low), "=d"(edx_high)
		//		: "c"(reg_fixed_addr_val));

		//count_in = ((long int)eax_low | (long int)edx_high << 32);
		//count_in = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
		//printk( KERN_INFO "fixed rdmsr [%d] in start:   %ld\n", i, count_in);
			
	}
	return count_in;
}

/* Disable counting */
static long pmu_stop_counters(void)
{
	long int count = 0;
	int i = 0;
	int countercheck;
	int copycount = 0;

	/* Disable counters on global counter control */
	__asm__("wrmsr"
			:
			: "c"(addr_global), "a"(0x00), "d"(0x00));

	/* Check CPU Switch */
	if(get_cpu() != hardware_events[7][counter - 1]){
			/* For multiple consecutive schedule out */
			if(mul_exit){
				copycount = 1;
			}
			/* For CPU switch */
			else{
					for(i = 0; i < 7; i++){
						if(i < 4){
								reg_addr_val = addr_val[i];
								__asm__("rdmsr"
										: "=a"(eax_low), "=d"(edx_high)
										: "c"(reg_addr_val));
								count = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
						}
						else{	
								reg_fixed_addr_val = addr_fixed_val[i-4];
								__asm__("rdmsr"
										: "=a"(eax_low), "=d"(edx_high)
										: "c"(reg_fixed_addr_val));
								count = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
						}
						countercheck = count - (hardware_events[i][counter-1]);
						if(countercheck < 0){
							copycount = 1;
							break;
						}
					}
			}
	}	

	/* Disable configurable counters */
	for (i = 0; i < 4; i++)
	{
		reg_addr_val = addr_val[i];
		reg_addr = addr[i];
		event_num = test_counters[i];
		event_on = event_num | umask | enable_bits;
		event_off = event_num | umask | disable_bits;

		__asm__("wrmsr"
				:
				: "c"(reg_addr), "a"(event_off), "d"(0x00));
		//Read for debugging
		//__asm__("rdmsr"
		//		: "=a"(eax_low), "=d"(edx_high)
		//		: "c"(reg_addr_val));
		//count = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
		
		//printk(KERN_INFO "Counter Stop [%d][%d] %ld \n", i, counter, count);
		
		/* Copy value over in case of CPU switch */
		if(copycount){
			/* User tapping data switch */
			if(counter == 0){	
				eax_low = temp_hpc[i];
			}
			/* Regular switch */
			else{
				eax_low = hardware_events[i][counter - 1];
			}
			__asm__("wrmsr"
				:
				: "c"(reg_addr_val), "a"(eax_low), "d"(edx_high));
		}

		//FOR DEBUGGING - NOTE: Causes inaccuracy of counters when
		//	enabled due to the overhead of printk
		//printk( KERN_INFO "rdmsr in:   %ld", count_in);
		//printk( KERN_INFO "rdmsr out:  %ld", count);
		//printk( KERN_INFO "rdmsr diff: %ld", count - count_in);
	}
	/* Copy value over in case of CPU switc for fixed counters */
	if(copycount){
		for (i = 0; i < 3; i++)
		{
			reg_fixed_addr_val = addr_fixed_val[i];
			if(counter == 0){
				eax_low = temp_hpc[i+4];
			}
			else{
				eax_low = hardware_events[i+4][counter - 1];
			}
			__asm__("wrmsr"
					:
					: "c"(reg_fixed_addr_val), "a"(eax_low), "d"(edx_high));		
		}
	}
	/* Disable fixed counters */
	__asm__("wrmsr"
			:
			: "c"(addr_fixed), "a"(0x000), "d"(0x00)); //disable:0 OS:1 User:2 Both:3

	mul_exit=1;
	return count;
}

/* Enable counting */
static long pmu_restart_counters(void)
{
	int i = 0;
	edx_high = 0x00;

	/* Enable counters on global counter control  */
	__asm__("wrmsr"
			:
			: "c"(addr_global), "a"(0x0f), "d"(0x07)); //4 HPCs 3 Fixed HPC

	/* Initialize first temp_hpc */
	if(pc_init){
		for(i=0; i < (num_events + 1); i++)
			temp_hpc[i] = 0;
		pc_init = 0;
	}

	/* Enable configuration counters */
	for (i = 0; i < 4; i++)
	{
		reg_addr_val = addr_val[i];
		reg_addr = addr[i];
		event_num = test_counters[i];
		event_on = event_num | umask | enable_bits;
		event_off = event_num | umask | disable_bits;

		/* Write last counter to PMU to keep value between CPU switch */
		if(counter == 0){
		
			eax_low = temp_hpc[i];
		}
		else{
			eax_low = hardware_events[i][counter - 1];
		}
		__asm__("wrmsr"
				:
				: "c"(reg_addr_val), "a"(eax_low), "d"(edx_high));

		//Read for debugging
		/*__asm__("rdmsr"
				: "=a"(eax_low), "=d"(edx_high)
				: "c"(reg_addr_val));
		count_in = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
		printk( KERN_INFO "rdmsr reset:   %ld", count_in);*/
		/* Enable counting */
		__asm__("wrmsr"
				:
				: "c"(reg_addr), "a"(event_on), "d"(0x00));
	}

	/* Enable fixed counters */
	__asm__("wrmsr"
			:
			: "c"(addr_fixed), "a"(0x222), "d"(0x00)); //disable:0 OS:1 User:2 Both:3
	
	for (i = 0; i < 3; i++)
	{
		reg_fixed_addr_val = addr_fixed_val[i];

		/* Write last counter to PMU to keep value between CPU switch */
		if(counter == 0){
			eax_low = temp_hpc[i+4];
		}
		else{
			eax_low = hardware_events[i+4][counter - 1];
		}
		__asm__("wrmsr"
				:
				: "c"(reg_fixed_addr_val), "a"(eax_low), "d"(edx_high));
		//Read for debugging
		/*__asm__("rdmsr"
				: "=a"(eax_low), "=d"(edx_high)
				: "c"(reg_fixed_addr_val));

		count_in = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
		printk( KERN_INFO "fixed rdmsr [%d] in restart:   %ld", i, count_in);*/
	}
	mul_exit=0;
	return reg_addr_val;
}

/* Read counters value */
static long pmu_read_counters(void)
{
	long int count;
	int i = 0;
	/* Read configuration counters */
	for (i = 0; i < 4; i++)
	{
		reg_addr_val = addr_val[i];
		reg_addr = addr[i];
		event_num = test_counters[i];

		event_on = event_num | umask | enable_bits;
		event_off = event_num | umask | disable_bits;

		__asm__("rdmsr"
				: "=a"(eax_low), "=d"(edx_high)
				: "c"(reg_addr_val));
		count = ((uint64_t)eax_low | (uint64_t)edx_high << 32);

		hardware_events[i][counter] = count;	
	}

	/* Read fixed counters */
	for (i = 0; i < 3; i++)
	{
		reg_fixed_addr_val = addr_fixed_val[i];

		__asm__("rdmsr"
				: "=a"(eax_low), "=d"(edx_high)
				: "c"(reg_fixed_addr_val));
		count_in = ((uint64_t)eax_low | (uint64_t)edx_high << 32);
		
		hardware_events[i+4][counter] = count_in;
	}
		
	hardware_events[num_events-1][counter] = get_cpu();
	hardware_events[num_events][counter] = current->pid;

	return count;
}

/* Track process through context switch */
#ifdef UNLOCKED
struct rq *jprobes_handle_finish_task_switch(struct task_struct *prev)
#else
void jprobes_handle_finish_task_switch(struct rq *rq, struct task_struct *prev)
#endif
{
	if(recording && (counter < num_recordings) && start_init)
	{
		/* Process schedule in */
		if (current->pid == target_pid)
		{
			timer_restart = 1;

			/* Enable counters */
			pmu_restart_counters();

			/* First counter values */ 	
			if (counter < num_recordings && first_hpc == 0)
			{
				pmu_read_counters();
				++counter;
				++first_hpc;
			}

			/* Start timer */
			ktime_period_ns = ktime_set(0, delay_in_ns);
			hrtimer_start(&hr_timer, ktime_period_ns, HRTIMER_MODE_REL);
		}
		/* Detect fork process */
		else if (current->parent->pid == target_pid)
		{
				fork_check++;
				timer_restart=1;

				/* Swap process to monitor to child */
				target_pid = current->pid;

				//DEBUG
				//printk_d("F: Swapping target_pid to PID: [%d]\n",target_pid);

				/* Enable counters */
				pmu_restart_counters();

				/* Start timer */
				ktime_period_ns = ktime_set(0, delay_in_ns);
				hrtimer_start(&hr_timer, ktime_period_ns, HRTIMER_MODE_REL);
		}
		/* Process schedule out */
		else if (prev->pid == target_pid)
		{
			if(pc_init){
				//DEBUG
				//printk(KERN_INFO "********** Exit Before Start **********\n Target [%d]\n PID: [%s][%d][%ld]\n PPID: [%s][%d][%ld]\n PREV: [%s][%d][%ld]->[%s][%d][%ld]\n Counter[%d] CPU[%d] S_I[%d] PC_I[%d]\n", target_pid, current->comm, current->pid, current->state, current->parent->comm,current->parent->pid, current->parent->state, prev->comm, prev->pid, prev->state, current->comm, current->pid, current->state, counter, get_cpu(), start_init, pc_init);
			}
			else
			{
				/* Process has forked */
				if(fork_check!=0)
				{
					/* Child exit & Detect parent has already exited child attach to systemd pid 1 */
					if (prev->state == 64 && prev->parent->pid == 1)
					{
						/* Cancel timer */
						hrtimer_cancel(&hr_timer);

						/* Disable counters */
						pmu_stop_counters();

						timer_restart = 0;
						fork_check--;

						/* Read counters */
						if (counter < num_recordings)
						{
							pmu_read_counters();	
							hardware_events[num_events][counter] = prev->pid;		
							++counter;
						}
					}
					/* Child exit & Detect parent still running */
					else if (prev->state == 64 && prev->parent->pid!=1)
					{
						/* Swap process to monitor to parent */	
						target_pid = prev->parent->pid;

						/* Cancel timer */
						hrtimer_cancel(&hr_timer);

						/* Disable counters */
						pmu_stop_counters();

						timer_restart = 0;
						fork_check--;

						/* Read counters */
						if (counter < num_recordings)
						{
							pmu_read_counters();
							hardware_events[num_events][counter] = prev->pid;	
							++counter;
						}
					}
					/* Child schedule out */
					else{
						/* Cancel timer */
						hrtimer_cancel(&hr_timer);
						/* Disable counters */
						pmu_stop_counters();

						timer_restart = 0;

						/* Read counters */
						if (counter < num_recordings)
						{
							pmu_read_counters();
							hardware_events[num_events][counter] = prev->pid;	
							++counter;
						}
					}
				}
				/* Process schedule out */
				else{
					/* Cancel timer */
					hrtimer_cancel(&hr_timer);

					/* Disable counters */		
					pmu_stop_counters();

					timer_restart = 0;

					/* Read counters */
					if (counter < num_recordings)
					{
						pmu_read_counters();
						hardware_events[num_events][counter] = prev->pid;	
						++counter;
					}
				}
			}
		}
	}
	jprobe_return();
	return (NULL);
}

static struct jprobe finish_task_switch_jp = {
	.entry = jprobes_handle_finish_task_switch,
	.kp.symbol_name = "finish_task_switch",
};

void unregister_all(void)
{
	unregister_jprobe(&finish_task_switch_jp);
}

int register_all(void)
{
	/* Register probes */
	int ret = register_jprobe(&finish_task_switch_jp);
	if (ret < 0)
	{
		printk(KERN_INFO "Couldn't register 'finish_task_switch' jprobe\n");
		unregister_all();
		return (-EFAULT);
	}

	return (0);
}

/* Restart timer */
enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
	ktime_t kt_now;

	/* Restart timer */
	if (timer_restart && (counter < num_recordings))
	{
		/* Read counter */
		pmu_read_counters();
		++counter;

		/* Forward timer */
		kt_now = hrtimer_cb_get_time(&hr_timer);
		hrtimer_forward(&hr_timer, kt_now, ktime_period_ns);

		return HRTIMER_RESTART;
	}
	/* No restart timer */
	else
	{
		if (!timer_restart)
		{
			printk("Timer Expired\n");
		}
		else
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
		pc_init=1;
		start_init=1;
		mul_exit=0;
		first_hpc=0;
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
	pmu_stop_counters();

	recording = 0;
	timer_restart = 0;
	pc_init = 1;
	fork_check = 0;
	first_hpc = 0;
	start_init=0;
	mul_exit=0;

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
	int i = 0;

	/* For periodic data extraction */
	if(recording){
		//FOR DEBUG
		//printk("Counter INT: %d On CPU: %d\n", counter, get_cpu());
		/* Read and save latest HPC */
		if(counter != 0){		
			for (i = 0; i < num_events + 1; i++)
			{
				temp_hpc[i]=hardware_events[i][counter-1];
			}

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
	else	
	{
		error_count = copy_to_user(buffer, hardware_events[0], size_of_message);
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
			printk(KERN_INFO "target pid: %d\n", (int)target_pid);
			//DEBUG
			//printk(KERN_INFO "%d %d %d %d %llu %d\n",kleb_ioctl_args.counter1, kleb_ioctl_args.counter2, kleb_ioctl_args.counter3,kleb_ioctl_args.counter4, kleb_ioctl_args.counter_umask, kleb_ioctl_args.user_os_rec);
			start_counters(kleb_ioctl_args.counter1, kleb_ioctl_args.counter2, kleb_ioctl_args.counter3,kleb_ioctl_args.counter4, kleb_ioctl_args.counter_umask, kleb_ioctl_args.user_os_rec);
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
	num_events = 8;

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
	return 0;
}

int initialize_timer()
{
	printk("Timer initializing\n");

	counter = 0;
	timer_restart = 0;

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &hrtimer_callback;

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

	if (initialize_memory() < 0)
	{
		printk(KERN_INFO "Memory failed to initialize");
		return (-ENODEV);
	}

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

	return 0;
}

int cleanup_timer()
{
	int ret;

	printk("Timer cleaning up\n");

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
	if (cleanup_memory() < 0)
	{
		printk(KERN_INFO "Memory failed to cleanup cleanly");
	}

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
