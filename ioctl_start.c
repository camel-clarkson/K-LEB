#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "kleb.h"

int main(){
	


	//int i = 0;
	//while(i<1000000000){ ++i; }
	//while(i<100000000){ ++i; }
	//while(i<10000000){ ++i; }
	//while(i<2000000){ ++i; }	
	//while(i<6000000){ ++i; } // same processing time as command below
	
	lprof_cmd_t lprof_cmd;
	int pid = fork();
	
	if(pid == 0){ // child
		sleep(1);
		printf("my pid (child): %d\n", getpid());
		execv("./test_perf", (char *[]){"james", NULL} );
	}else{
		int fd = open("/dev/temp", O_RDWR);
		if(fd == -1){
			printf("Error in opening file \n");
			exit(-1);
		}
					
		lprof_cmd.pid = pid;
		printf("child pid: %d\n", pid);
		//lprof_cmd.counter = 0x020b;
		//lprof_cmd.counter = 0x412e;
		lprof_cmd.counter = 0xc0;
		lprof_cmd.config = 0;

		char *msg = "Starting stuff";
		ioctl(fd, IOCTL_START, &lprof_cmd);

		int i = 0;
		//while(i<1000000000){ ++i; }
		wait(pid);
		close(fd);
	}

}
