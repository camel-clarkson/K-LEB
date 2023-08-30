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

#ifndef KLEB_H
#define KLEB_H

#include <linux/ioctl.h>

#define IOC_MAGIC 'k'

#define IOCTL_DEFINE_COUNTERS _IOW(IOC_MAGIC, 0, char *)
#define IOCTL_START _IOW(IOC_MAGIC, 1, char *)
#define IOCTL_DUMP _IOW(IOC_MAGIC, 2, char *)
#define IOCTL_STOP _IOW(IOC_MAGIC, 3, char *)
#define IOCTL_DELETE_COUNTERS _IOW(IOC_MAGIC, 4, char *)
#define IOCTL_DEBUG _IOW(IOC_MAGIC, 5, char *)
#define IOCTL_STATS _IOW(IOC_MAGIC, 6, char *)

#define DEVICE_NAME "kleb"

#define DEVICE_PATH "/dev/" DEVICE_NAME

/* Branch Events */
#define BR_RET 0x00c2
#define BR_MISP_RET 0x00c3
/* Cache Events */
#define LOAD 0x0129
#define STORE 0x0229
#define L1_ICACHE_MISS 0x1060
#define MISS_LLC 0xff9a
/* Instructions Events */
#define INST_RET 0x00c0
#define NEAR_RET 0x00c8
#define INST_FP 0xff03
#define ARITH_MULT 0x0203
#define ARITH_DIV 0x0403
#define L1_MISS_DTLB 0xff45
#define STLB_HIT 0x0084
#define UNKNOWN_EVENT 0xffff

/* K-LEB parameters */
typedef struct {
	int pid;
	unsigned int counter[10];
	unsigned int num_events;
	unsigned int delay_in_ns;
	unsigned int user_os_rec; // 1 is user only, 2 is os only, 3 is both	
} kleb_ioctl_args_t;

int initialize_memory( void );
int initialize_timer( void );
int initialize_ioctl( void );
int init_module( void );

int start_counters( void );
int stop_counters( void );
int dump_counters( void );

int cleanup_memory( void );
int cleanup_timer( void );
int cleanup_ioctl( void );
void cleanup_module( void );

#define DO_EXIT_NAME "finish_task_switch"
#define DO_ENTER_NAME "prepare_arch_switch"
#define FINISH_TASK_SWITCH_NAME "finish_task_switch"

#define DEBUG
#ifdef DEBUG
	#define printk_d(...) printk(KERN_INFO "kleb: " __VA_ARGS__)
#else
	#define printk_d(...)
#endif // DEBUG

#endif // KLEB_H
