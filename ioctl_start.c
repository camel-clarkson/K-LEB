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

	char *msg = "Starting stuff";
	ioctl(fd, IOCTL_START, getpid());
	printf("my pid: %d\n", getpid());

	int i = 0;
	//while(i<1000000000){ ++i; }
	while(i<10000000){ ++i; }

	close(fd);
}
