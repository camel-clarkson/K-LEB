#ifndef KLEB_H
#define KLEB_H

#include <linux/ioctl.h>

#define IOC_MAGIC 'k'

#define IOCTL_DEFINE_COUNTERS _IO(IOC_MAGIC, 0)
#define IOCTL_START _IO(IOC_MAGIC, 1)
#define IOCTL_STOP _IO(IOC_MAGIC, 2)
#define IOCTL_DELETE_COUNTERS _IO(IOC_MAGIC, 3)
#define IOCTL_DEBUG _IO(IOC_MAGIC, 4)
#define IOCTL_STATS _IO(IOC_MAGIC, 5)

#define DEVICE_NAME "kleb"

#define DEVICE_PATH "/dev/" DEVICE_NAME

int initialize_memory( void );
int initialize_timer( void );
int initialize_ioctl( void );

int start_counters( void );
int stop_counters( void );

int cleanup_memory( void );
int cleanup_timer( void );
int cleanup_ioctl( void );

#endif // KLEB_H
