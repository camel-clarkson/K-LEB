/* Copyright (c) 2017, 2020 James Bruska, Caleb DeLaBruere, Chutitep Woralert

This file is part of K-LEB.

K-LEB is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

K-LEB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with K-LEB.  If not, see <https://www.gnu.org/licenses/>. */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include "kleb.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>

static int checkint;

void sigintHandler(int sig_num){
	signal(SIGINT, sigintHandler);
	printf("Stop monitoring....\n");
	checkint=1;
}
unsigned int NameToRawConfigMask(char* event_name)
{
	/* Branch Events **/
	if      (strcmp(event_name,"BR_RET") == 0) return BR_RET;
	else if (strcmp(event_name,"BR_MISP_RET") == 0) return BR_MISP_RET;
	else if (strcmp(event_name,"BR_EXEC") == 0) return BR_EXEC;
	else if (strcmp(event_name,"MISP_BR_ANY") == 0) return MISP_BR_ANY;
	else if (strcmp(event_name,"MISP_BR_UN") == 0) return MISP_BR_UN;
	else if (strcmp(event_name,"MISP_BR_C") == 0) return MISP_BR_C;
	/** Cache Events **/
	else if (strcmp(event_name,"LOAD") == 0) return LOAD;
	else if (strcmp(event_name,"STORE") == 0) return STORE;
	else if (strcmp(event_name,"L1_ICACHE_STALL") == 0) return L1_ICACHE_STALL;
	else if (strcmp(event_name,"L1_ICACHE_REF") == 0) return L1_ICACHE_REF;
	else if (strcmp(event_name,"L1_ICACHE_MISS") == 0) return L1_ICACHE_MISS;
	else if (strcmp(event_name,"L1_ICACHE_HIT") == 0) return L1_ICACHE_HIT;
	else if (strcmp(event_name,"L1_DCACHE_REF") == 0) return L1_DCACHE_REF;
	else if (strcmp(event_name,"L1_DCACHE_MISS") == 0) return L1_DCACHE_MISS;
	else if (strcmp(event_name,"L1_DCACHE_HIT") == 0) return L1_DCACHE_HIT;
	else if (strcmp(event_name,"L2_DATA_REF") == 0) return L2_DATA_REF;
	else if (strcmp(event_name,"L2_DATA_HIT") == 0) return L2_DATA_HIT;
	else if (strcmp(event_name,"LLC") == 0) return LLC;
	else if (strcmp(event_name,"MISS_LLC") == 0) return MISS_LLC;
	else if (strcmp(event_name,"MEM_LOAD_RETIRED_LLC_MISS") == 0) return MEM_LOAD_RETIRED_LLC_MISS;
	/** Instruction Events **/
	else if (strcmp(event_name,"INST_FP") == 0) return INST_FP;
	else if (strcmp(event_name,"ARITH_MULT") == 0) return ARITH_MULT;
	else if (strcmp(event_name,"ARITH_DIV") == 0) return ARITH_DIV;
	/** Proc calls Events **/
	else if (strcmp(event_name,"CALL") == 0) return CALL;
	else if (strcmp(event_name,"CALL_D_EXEC") == 0) return CALL_D_EXEC;
	else if (strcmp(event_name,"CALL_ID_EXEC") == 0) return CALL_ID_EXEC;
	else if (strcmp(event_name,"MISP_CALL") == 0) return MISP_CALL;
	/** TLB Events **/
	else if (strcmp(event_name,"MISS_ITLB") == 0) return MISS_ITLB;
	else if (strcmp(event_name,"MISS_DTLB") == 0) return MISS_DTLB;
	else if (strcmp(event_name,"STLB_HIT") == 0) return STLB_HIT;
	/* UNKNOWN Event */
	else return UNKNOWN_EVENT;
}
int main(int argc, char **argv)
{
	//FILE *config;
	// ARGS: Counter1, Counter2, Counter3, Counter4, Umask, Timer Delay (in ms), Log path, Program path
	// EX: 0xc0 0x4f 100000
	if(argc < 9){
		printf("Error reading configurations\nExiting...\n");
		exit(0);
	}
	int usrcmd;
	int checkempty;
	float hrtimer = 1;
	kleb_ioctl_args_t kleb_ioctl_args;
	unsigned int counter[4];
	int pid = atoi(argv[8]);
	
	checkint=0;
	signal(SIGINT, sigintHandler);

	if(!kill(pid, 0) && pid != 0){
		usrcmd = 0;
	}
	else{
		//printf("forking for %s \n",argv[7]);
		usrcmd = 1;
		pid = fork();
	}

	if (pid == 0)
	{ // child
		sleep(1);
		//printf("my fork pid (child): %d\n", getpid());
		char* cmdargv[argc-8];
		for (int i = 9; i < argc; i++){
			cmdargv[i-9] = argv[i];
			printf("Argv: %s Argc:%d\n",cmdargv[i-9], argc);
		}
		cmdargv[argc-9]=NULL;
		//if(strcmp(argv[8], "/home/camel/chutitep/hpc-overhead-test/linpack/xlinpack_xeon64 ")){
		//	execv(argv[8], (char *[]){" -i ", "/home/camel/chutitep/hpc-overhead-test/linpack/data_file", NULL});
		//}
		//else{
			//execv(argv[8], (char *[]){"james", NULL});
			execv(argv[8], cmdargv);
		//}
		

		
	}
	else
	{
		//printf("DEVICE_PATH: %s\n", DEVICE_PATH);
		int fd = open(DEVICE_PATH, O_RDWR);
		if (fd == -1)
		{
			printf("Error in opening file \n");
			perror("Emesg");
			exit(-1);
		}
		kleb_ioctl_args.pid = pid;
		//printf("child pid[%s]: %d\n",argv[0], pid);
		//printf("Argv: %s Argc:%d\n",argv[8], argc);
		for(int i = 1; i < 5; i++){
			counter[i-1] = NameToRawConfigMask(argv[i]);
			//printf("Counter[%d] %x\n",i, counter[i-1]);
		}
		kleb_ioctl_args.counter1 = counter[0];
		kleb_ioctl_args.counter2 = counter[1];
		kleb_ioctl_args.counter3 = counter[2];
		kleb_ioctl_args.counter4 = counter[3];

		//printf("Counter %x %x %x %x", kleb_ioctl_args.counter1,kleb_ioctl_args.counter2,kleb_ioctl_args.counter3,kleb_ioctl_args.counter4);
		/* kleb_ioctl_args.counter1 = strtol(argv[1], NULL, 16);
		kleb_ioctl_args.counter2 = strtol(argv[2], NULL, 16);
		kleb_ioctl_args.counter3 = strtol(argv[3], NULL, 16);
		kleb_ioctl_args.counter4 = strtol(argv[4], NULL, 16);*/
		kleb_ioctl_args.counter_umask = strtol(argv[5], NULL, 16); 
		hrtimer = strtof(argv[6], NULL);
		kleb_ioctl_args.delay_in_ns = hrtimer*1000000;
		//kleb_ioctl_args.delay_in_ns = strtol(argv[6], NULL, 10);
		kleb_ioctl_args.user_os_rec = 1; //User:1 OS:2 Both:3
		
		char *msg = "Starting stuff";
		printf("Starting K-LEB...\n");
		//printf("IOCTL_START\n");
		//ioctl(fd, IOCTL_START, &kleb_ioctl_args);
		////////
		   if(ioctl(fd, IOCTL_START, &kleb_ioctl_args) < 0)
		{
			     printf("ioctl failed and returned errno %s \n",strerror(errno));
		}


		//printf("After IOCTL_START\n");
		
		

				
		int status = 0;
		struct timespec t1, t2;

		struct timeval tv1, tv2;
		unsigned long long int tap_time;
		//value no absolute should reduce a bit to prevent counter overflow
		//t1.tv_nsec = 10000000L;
		//t1.tv_nsec = 40000000L;
		tap_time = kleb_ioctl_args.delay_in_ns*250;
		if(tap_time >= 1000000000 ){
			t1.tv_sec = tap_time/1000000000;
			t1.tv_nsec = tap_time-(t1.tv_sec*1000000000);
		}
		else{
			t1.tv_sec = 0;
			t1.tv_nsec = tap_time;
		}
		//printf("Tapping every: %ld sec %ld nsec\n", t1.tv_sec, t1.tv_nsec);
				int num_events = 8;
				int num_recordings = 500;
				unsigned int **hardware_events = malloc( (num_events+1)*sizeof(unsigned int *) );
				hardware_events[0] = malloc( (num_events+1)*num_recordings*sizeof(unsigned int) );
								
				int j;
				int i;
				int k;
				int num_sample = 0;
				int num_tap = 0;
				int bufindex = 0;
				//int bufmax = 40000;
				
				for ( i=0; i < (num_events+1); ++i ) { // This reduces the number of malloc calls
				 hardware_events[i] = *hardware_events + num_recordings * i;
		   		}
				//printf("log_path %s", argv[8]);
				//Double buffer
				long int hardware_events_buffer[9][40000];
				long int temp_overflow[7];
				int size_of_message = num_recordings * (num_events+1) * sizeof(long int);
				
				if(!usrcmd){
					printf("Monitoring HPC... \nWait for daemon pid %d \nPress Ctrl+C to exit\n", pid);	
					while (!kill(pid, 0) && !checkint) {
						
						nanosleep(&t1, &t2);
						int ret = read(fd, hardware_events[0], size_of_message);
						if( ret != 0){
							//printf("ret: %d\n", ret);
							checkempty = 0;
							++num_tap;
						}
						if (ret == 0){
							//printf("Empty string\n");
						}
						else{
							for ( j=0; j < num_recordings && !checkempty; ++j ) {
								for ( i=0; i < (num_events+1) && !checkempty; ++i ) { 
									if(hardware_events[i][j]==-10)
									{
										checkempty = 1;
										break;
									}
									else{
										//printf("%d,", hardware_events[i][j]);
										hardware_events_buffer[i][bufindex] = hardware_events[i][j];
										++num_sample;
									}
								}
								if(!checkempty)
									++bufindex;
								//printf("\n");
							}
						}
					}
				}
				else{
					printf("Monitoring HPC... \nWait for pid %d to exit\n", pid);
					while (!waitpid(pid, &status, WNOHANG)) {
						
						nanosleep(&t1, &t2);
						int ret = read(fd, hardware_events[0], size_of_message);
						if( ret != 0){
							//printf("ret: %d\n", ret);
							checkempty = 0;
							++num_tap;
						}

						if (ret == 0){
							//printf("Empty string\n");
						}
						else{
							for ( j=0; j < num_recordings && !checkempty; ++j ) {
								for ( i=0; i < (num_events+1) && !checkempty; ++i ) { 
									if(hardware_events[i][j]==-10)
									{
										checkempty = 1;
										break;
									}
									else{
										//printf("%d,", hardware_events[i][j]);
										hardware_events_buffer[i][bufindex] = hardware_events[i][j];
										++num_sample;
									}
								
									//hardware_events[i][j] = 0;
								}
								if(!checkempty)
									++bufindex;
								//printf("\n");
								
							}
						}
					 
					}//while
				}//else
				
ioctl_stop:				//IOCTL STOP
				printf("Finish monitoring...\n");
				ioctl(fd, IOCTL_STOP, "Stopping stuff");
				int ret_out = read(fd, hardware_events[0], size_of_message);
				if (ret_out < 0)
				{
					perror("Failed to read the message from the device.\n");
					return errno;
				}
				else{
					checkempty = 0;
				}
				for (j = 0; j < num_recordings && !checkempty; ++j)
				{
					for (i = 0; i < (num_events + 1) && !checkempty; ++i)
					{
						if(hardware_events[i][j]==-10)
						{
							checkempty = 1;
							break;
						}						
						else{
							//printf("%d,", hardware_events[i][j]);
							hardware_events_buffer[i][bufindex] = hardware_events[i][j];
							++num_sample;
						}
					}
					if(!checkempty)
									++bufindex;
				}
				checkempty = 0;
				printf("Stopping K-LEB...\n# of Sample: %d\n", num_sample/9);
				
				FILE *fp = fopen(argv[7], "a");
				//Print double buffer
				fprintf(fp, "%s,%s,%s,%s,INST,CPU_CLK_CYCLE,CPU_REF_CYCLE,CPU,PID\n", argv[1], argv[2], argv[3], argv[4]);
				for (j = 0; j < bufindex && !checkempty; ++j)
				{
					for (i = 0; i < (num_events + 1) && !checkempty; ++i)
					{
						//printf("%ld,", hardware_events_buffer[i][j]);
						if(j==0){
							//fprintf(stdout, "%ld,", hardware_events_buffer[i][j]);
							fprintf(fp, "%ld,", hardware_events_buffer[i][j]);
						}
						else if(hardware_events_buffer[4][j]==0 && hardware_events_buffer[5][j]==0 && hardware_events_buffer[6][j]==0 && hardware_events_buffer[7][j]==0 && hardware_events_buffer[8][j]==0){
							printf("Sample is empty\n ");
							checkempty = 1;
							break;
						}
						else{
							if(i < 7 && hardware_events_buffer[i][j] < hardware_events_buffer[i][j-1]){
								temp_overflow[i] += 4294967295;
								
							}
							if(i < 7){
								//fprintf(stdout, "%ld,", hardware_events_buffer[i][j] + temp_overflow[i]);
								fprintf(fp, "%ld,", hardware_events_buffer[i][j] + temp_overflow[i]);
							}
							else
							{
								//fprintf(stdout, "%ld,", hardware_events_buffer[i][j]);
								fprintf(fp, "%ld,", hardware_events_buffer[i][j]);
							}
							
								
							
							
						}
					}
					//fprintf(stdout, "\n");
					fprintf(fp, "\n");
				}
				//fprintf(stdout, "\n");
				fprintf(fp, "\n");

				//close(fp);
	
		
		//wait(pid);
		close(fd);
		printf("Log Path: %s\n ", argv[7]);
		
	}
}
