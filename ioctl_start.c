#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "kleb.h"

int main(int argc, char **argv){

// ARGS: Counter, Umask, Timer Delay (in NS), 
// EX: 0xc0 0x4f 100000
	kleb_ioctl_args_t kleb_ioctl_args;
	int pid = fork();
	
	if(pid == 0){ // child
		sleep(1);
		printf("my pid (child): %d\n", getpid());
		//char *const args[] = {"-cvf", "tar-archive-test.tar", "large-file"}; 
		execv(argv[4], (char *[]){"james", NULL} );
	}else{
		int fd = open(DEVICE_PATH, O_RDWR);
		if(fd == -1){
			printf("Error in opening file \n");
			exit(-1);
		}
					
		kleb_ioctl_args.pid = pid;
		printf("child pid: %d\n", pid);
		kleb_ioctl_args.counter = strtol(argv[1], NULL, 16); // Other options: 0x020b, 0x412e
		kleb_ioctl_args.counter_umask = strtol(argv[2], NULL, 16);
		kleb_ioctl_args.delay_in_ns = strtol(argv[3], NULL, 10);
		kleb_ioctl_args.user_os_rec = 3;
		
		// printf("%x\n%x\n%llu\n", kleb_ioctl_args.counter, kleb_ioctl_args.counter_umask, kleb_ioctl_args.delay_in_ns);
		char *msg = "Starting stuff";
		ioctl(fd, IOCTL_START, &kleb_ioctl_args);

		wait(pid);
		close(fd);
	}

}
