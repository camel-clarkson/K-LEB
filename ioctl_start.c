#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
//#include "time.h"
#include "kleb.h"
//#include <errno.h>
//#include <sys/types.h>
//#include <sys/wait.h>


int main(int argc, char **argv){
	//FILE *config;
// ARGS: Counter1, Counter2, Counter3, Counter4, Umask, Timer Delay (in NS), 
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
		
		//for(int i=0;i<4;i++){
			//fscanf(config,"%d",&kleb_ioctl_args.counter[i])
			//kleb_ioctl_args.counter[i] = strtol(argv[1+i], NULL, 16); // Other options: 0x020b, 0x412e
		//}
		kleb_ioctl_args.counter = strtol(argv[1], NULL, 16); 
		kleb_ioctl_args.counter_umask = strtol(argv[2], NULL, 16);
		kleb_ioctl_args.delay_in_ns = strtol(argv[3], NULL, 10);
		kleb_ioctl_args.user_os_rec = 3;
		
		// printf("%x\n%x\n%llu\n", kleb_ioctl_args.counter, kleb_ioctl_args.counter_umask, kleb_ioctl_args.delay_in_ns);
		char *msg = "Starting stuff";
		ioctl(fd, IOCTL_START, &kleb_ioctl_args);

/*		
		int status;
		while (!waitpid(pid, &status, WNOHANG)) {
				sleep(.5);
				//ioctl(fd, IOCTL_DUMP, "Dumping");
				int num_events = 5;
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
				int j;
				//for ( j=0; j < 100; ++j ) {
				for ( j=0; j < num_recordings; ++j ) {
					for ( i=0; i < (num_events+1); ++i ) { 
						fprintf(stdout, "%d,", hardware_events[i][j]);
					}
					fprintf(stdout, "\n");
				}
				fprintf(stdout, "\n");
					
		}
		*/
		wait(pid);
		close(fd);
	}

}
