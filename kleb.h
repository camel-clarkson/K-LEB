#include <linux/ioctl.h>

#define IOC_MAGIC 'k'
#define IOCTL_HELLO _IO(IOC_MAGIC, 0)

int initialize_memory( void );
int initialize_timer( void );
int initialize_ioctl( void );

int cleanup_memory( void );
int cleanup_timer( void );
int cleanup_ioctl( void );


