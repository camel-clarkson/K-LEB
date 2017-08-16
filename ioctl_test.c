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

	ioctl(fd, IOCTL_HELLO);

	close(fd);
}
