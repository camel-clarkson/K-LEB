#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "kleb.h"

int main(){
	
	kleb_ioctl_args_t kleb_ioctl_args;
	int pid = fork();
	
	if(pid == 0){ // child
		sleep(1);
		printf("my pid (child): %d\n", getpid());
		execv("./test_perf", (char *[]){"james", NULL} );
	}else{
		int fd = open(DEVICE_PATH, O_RDWR);
		if(fd == -1){
			printf("Error in opening file \n");
			exit(-1);
		}
					
		kleb_ioctl_args.pid = pid;
		printf("child pid: %d\n", pid);
		kleb_ioctl_args.counter = 0xc0; // Other options: 0x020b, 0x412e
		kleb_ioctl_args.counter_umask = 0x3F;
		kleb_ioctl_args.delay_in_ns = 1E5L;
		kleb_ioctl_args.user_os_rec = 3;

		char *msg = "Starting stuff";
		ioctl(fd, IOCTL_START, &kleb_ioctl_args);

		wait(pid);
		close(fd);
	}

}
