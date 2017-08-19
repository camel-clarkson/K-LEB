#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "kleb.h"

#define BUFFER_LENGTH 16

static int receive[BUFFER_LENGTH];

int main(){
	int fd = open("/dev/temp", O_RDWR);
	if(fd == -1){
		printf("Error in opening file \n");
		exit(-1);
	}

	ioctl(fd, IOCTL_STOP);

	int ret = read(fd, (char *) receive, BUFFER_LENGTH);
  if (ret < 0){ 
    perror("Failed to read the message from the device.\n");
    return errno;
  }   

	int i;
	for(i=0; i<5; ++i){ printf("%d ", receive[i]); }
	printf("\n");
  //printf("The received message is: [%s]\n", receive);

	close(fd);
}
