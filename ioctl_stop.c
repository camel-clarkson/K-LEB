#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "kleb.h"

#include "time.h"
#include "stdint.h"
#define BILLION 1000000000L

int main(){
	int fd = open(DEVICE_PATH, O_RDWR);
	if(fd == -1){
		printf("Error in opening file \n");
		exit(-1);
	}

	//struct timespec start, end;
	//clock_gettime(CLOCK_MONOTONIC, &start);

	ioctl(fd, IOCTL_STOP, "Stopping stuff");

	int num_events = 7;
	int num_recordings = 500;
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

	FILE *fp = fopen("../LR_v13_tfdeploy/input.csv", "a");
	fprintf(stdout, "1,foo,\n");
	int j;
	//for ( j=0; j < 100; ++j ) {
	for ( j=0; j < num_recordings; ++j ) {
		for ( i=0; i < (num_events+1); ++i ) { 
			fprintf(stdout, "%d,", hardware_events[i][j]); 
		}
		fprintf(stdout, "\n"); 
	}
	fprintf(stdout, "\n");

//	for ( j=0; j < 50; ++j ) {
//		for ( i=0; i < 1; ++i ) { 
//  			fprintf(stdout, "%d ", hardware_events[i][j]); 
//		}
//		fprintf(stdout, "\n");
//	}
//	fprintf(stdout, "\n");

//	printf("The received message is: [%s]\n", receive);

	close(fd);

	//system("./python_test");
	//chdir("../LR_v13_tfdeploy");
  //system("su bruskajp -c \"python3 lr_script_OC_testing_only_2.py\"");
	
	//clock_gettime(CLOCK_MONOTONIC, &end);
	//uint64_t diff1 = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
	//fprintf(stdout, "elapsed time = %llu nanoseconds\n", (long long unsigned int) diff1);

}


