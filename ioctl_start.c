#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "kleb.h"

int main(){
	int fd = open("/dev/temp", O_RDWR);
	if(fd == -1){
		printf("Error in opening file \n");
		exit(-1);
	}

	lprof_cmd_t lprof_cmd;
	lprof_cmd.pid = getpid();
	//lprof_cmd.counter = 0x020b;
	//lprof_cmd.counter = 0x412e;
	lprof_cmd.counter = 0xc0;
	lprof_cmd.config = 0;

	char *msg = "Starting stuff";
	ioctl(fd, IOCTL_START, &lprof_cmd);
	printf("my pid: %d\n", getpid());

	int i = 0;
	//while(i<1000000000){ ++i; }
	while(i<100000000){ ++i; }
	//while(i<10000000){ ++i; }
	//while(i<5000000){ ++i; }

	close(fd);
}
