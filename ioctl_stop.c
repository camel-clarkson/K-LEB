#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "kleb.h"

int main(){
	int fd = open("/dev/temp", O_RDWR);
	if(fd == -1){
		printf("Error in opening file \n");
		exit(-1);
	}

	ioctl(fd, IOCTL_STOP, "Stopping stuff");

	int num_events = 4;
	int num_recordings = 100;
	int **hardware_events = malloc( (num_events+1)*sizeof(int *) );
	hardware_events[0] = malloc( (num_events+1)*num_recordings*sizeof(int) );
	
	int i;
	for ( i=0; i < (num_events+1); ++i ) { // This reduces the number of malloc calls
    hardware_events[i] = *hardware_events + num_recordings * i;
  }
	
	int size_of_message = num_recordings * (num_events+1) * sizeof(int);
	int ret = read(fd, hardware_events[0], size_of_message);
  if (ret < 0){ 
    perror("Failed to read the message from the device.\n");
    return errno;
  } 

	int j;
	for ( i=0; i < (num_events+1); ++i ) { 
		for ( j=0; j < num_recordings; ++j ) {
		//for ( j=0; j < 10; ++j ) {
			printf("%d ", hardware_events[i][j]); 
		}
		printf("\n\n");
	}
	printf("\n");
  //printf("The received message is: [%s]\n", receive);

	close(fd);
}
