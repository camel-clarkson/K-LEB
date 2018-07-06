#ifndef KLEB_H
#define KLEB_H

#include <linux/ioctl.h>

#define IOC_MAGIC 'k'

#define IOCTL_DEFINE_COUNTERS _IOW(IOC_MAGIC, 0, char *)
#define IOCTL_START _IOW(IOC_MAGIC, 1, char *)
#define IOCTL_STOP _IOW(IOC_MAGIC, 2, char *)
#define IOCTL_DELETE_COUNTERS _IOW(IOC_MAGIC, 3, char *)
#define IOCTL_DEBUG _IOW(IOC_MAGIC, 4, char *)
#define IOCTL_STATS _IOW(IOC_MAGIC, 5, char *)

#define DEVICE_NAME "kleb"

#define DEVICE_PATH "/dev/" DEVICE_NAME

typedef struct {
	int pid;
  unsigned int counter;
	unsigned long long counter_umask;
	unsigned int delay_in_ns;
	unsigned int user_os_rec; // 1 is user only, 2 is os only, 3 is both	
} kleb_ioctl_args_t;

int initialize_memory( void );
int initialize_timer( void );
int initialize_ioctl( void );
int init_module( void );

int start_counters( unsigned int pmu_counter, unsigned long long pmu_config, unsigned int pmu_user_os_rec);
int stop_counters( void );

int cleanup_memory( void );
int cleanup_timer( void );
int cleanup_ioctl( void );
void cleanup_module( void );

#define DO_EXIT_NAME "do_exit"
#define COPY_PROCESS_NAME "copy_process"
#define UPROBE_COPY_PROCESS_NAME "uprobe_copy_process"
#define FINISH_TASK_SWITCH_NAME "finish_task_switch"

#define DEBUG
#ifdef DEBUG
	#define printk_d(...) printk(KERN_INFO "kleb: " __VA_ARGS__)
#else
	#define printk_d(...)
#endif // DEBUG

#endif // KLEB_H
