/* Copyright (c) 2017, 2020 Chutitep Woralert, James Bruska, Caleb DeLaBruere, Chen Liu

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

/* Check interrupt */
static int checkint;

/* Handle interrupt */
void sigintHandler(int sig_num){
	signal(SIGINT, sigintHandler);
	printf("Stop monitoring....\n");
	checkint=1;
}
/* Convert event name to event code */
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
	// ARGS: Counter1, Counter2, Counter3, Counter4, Timer Delay (in ms), Log path, Program path
	if(argc < 8){
		printf("Error reading configurations\nExiting...\n");
		exit(0);
	}

	int usrcmd;
	int checkempty;
	float hrtimer = 1;
	kleb_ioctl_args_t kleb_ioctl_args;
	unsigned int counter[4];
	int pid = atoi(argv[7]);

	checkint=0;
	signal(SIGINT, sigintHandler);

	
	if(!kill(pid, 0) && pid != 0){
		/* User pass in program pid */
		usrcmd = 0;
	}	
	else{
		/* User pass in program path */
		usrcmd = 1;
		pid = fork();
	}

	if (pid == 0)
	{ 
		/* Execute child */
		sleep(1);
		/* Phrase program arguments */
		char* cmdargv[argc-7];
		for (int i = 8; i < argc; i++){
			cmdargv[i-8] = argv[i];
		}
		cmdargv[argc-8]=NULL;
		execv(argv[7], cmdargv);
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

		/* Set pid to monitor */
		kleb_ioctl_args.pid = pid;

		/* Convert events */
		for(int i = 1; i < 5; i++){
			if(isalpha(argv[i][0])){
				/* Convert event name to event code */
				counter[i-1] = NameToRawConfigMask(argv[i]);
			}
			else{
				/* Directly use event code */
				counter[i-1] = strtol(argv[i], NULL, 16);
			}
		}
		kleb_ioctl_args.counter1 = counter[0];
		kleb_ioctl_args.counter2 = counter[1];
		kleb_ioctl_args.counter3 = counter[2];
		kleb_ioctl_args.counter4 = counter[3];
		kleb_ioctl_args.counter_umask = strtol("0x00", NULL, 16); 

		/* Set timer to ns */
		hrtimer = strtof(argv[5], NULL);
		kleb_ioctl_args.delay_in_ns = hrtimer*1000000;

		/* Set montitor mode */
		kleb_ioctl_args.user_os_rec = 1; //User:1 OS:2 Both:3
		
		printf("Starting K-LEB...\n");

	 	/* Start monitoring */
		if(ioctl(fd, IOCTL_START, &kleb_ioctl_args) < 0)
		{
			printf("ioctl failed and returned errno %s \n",strerror(errno));
		}
				
		int status = 0;
		struct timespec t1, t2;
		unsigned long long int tap_time;

		/* Set user tapping time */
		tap_time = kleb_ioctl_args.delay_in_ns*250;
		if(tap_time >= 1000000000 ){
			t1.tv_sec = tap_time/1000000000;
			t1.tv_nsec = tap_time-(t1.tv_sec*1000000000);
		}
		else{
			t1.tv_sec = 0;
			t1.tv_nsec = tap_time;
		}

		int num_events = 8;
		int num_recordings = 500;
		/* Buffer for tapping */
		unsigned int **hardware_events = malloc( (num_events+1)*sizeof(unsigned int *) );
		hardware_events[0] = malloc( (num_events+1)*num_recordings*sizeof(unsigned int) );
								
		int i, j;
		int num_sample = 0;
		int num_tap = 0;
		int bufindex = 0;
				
		for ( i=0; i < (num_events+1); ++i ) {
			hardware_events[i] = *hardware_events + num_recordings * i;
		}
		/* Buffer for storing data */
		long int hardware_events_buffer[9][40000];
		/* Overflow value */
		long int temp_overflow[7];
		int size_of_message = num_recordings * (num_events+1) * sizeof(long int);
				
		if(!usrcmd){
			/* Monitor pid */
			printf("Monitoring HPC... \nWait for pid %d \nPress Ctrl+C to exit\n", pid);	
			while (!kill(pid, 0) && !checkint) {

				nanosleep(&t1, &t2);

				/* Extract data from kernel */
				int ret = read(fd, hardware_events[0], size_of_message);
				if (ret < 0)
				{
					perror("Failed to read the message from the device.\n");
					return errno;
				}
				else if (ret == 0){
					/* Empty data */
					//printf("Empty string\n");
					checkempty = 1;
				}
				else{
					checkempty = 0;
					++num_tap;
					for ( j=0; j < num_recordings && !checkempty; ++j ) {
						for ( i=0; i < (num_events+1) && !checkempty; ++i ) { 
							if(hardware_events[i][j]==-10)
							{
								/* End of data buffer */
								checkempty = 1;
								break;
							}
							else{
								hardware_events_buffer[i][bufindex] = hardware_events[i][j];
							}
						}
						if(!checkempty)
							++bufindex;
							++num_sample;
					}
				}
			}
		}
		else{
			/* Monitor program path */
			printf("Monitoring HPC... \nWait for pid %d to exit\n", pid);
			while (!waitpid(pid, &status, WNOHANG)) {	
				nanosleep(&t1, &t2);
				/* Extract data from kernel */
				int ret = read(fd, hardware_events[0], size_of_message);
				if (ret < 0)
				{
					perror("Failed to read the message from the device.\n");
					return errno;
				}
				else if (ret == 0){
					/* Empty data */
					//printf("Empty string\n");
					checkempty = 1;
				}
				else{
					checkempty = 0;
					++num_tap;
					for ( j=0; j < num_recordings && !checkempty; ++j ) {
						for ( i=0; i < (num_events+1) && !checkempty; ++i ) { 
							if(hardware_events[i][j]==-10)
							{
								/* End of data buffer */
								checkempty = 1;
								break;
							}
							else{
								hardware_events_buffer[i][bufindex] = hardware_events[i][j];
							}
						}
						if(!checkempty){
							++bufindex;
							++num_sample;
						}								
					}
				}
			}
		}
				
		printf("Finish monitoring...\n");
		/* Stop monitoring */
		if(ioctl(fd, IOCTL_STOP, "Stopping") < 0)
		{
			printf("ioctl failed and returned errno %s \n",strerror(errno));
		}
		/* Extract last batch of data */
		int ret_out = read(fd, hardware_events[0], size_of_message);
		if (ret_out < 0)
		{
			perror("Failed to read the message from the device.\n");
			return errno;
		}
		else if (ret_out == 0){
			/* Empty data */
			checkempty = 1;
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
					/* End of data buffer */
					checkempty = 1;
					break;
				}						
				else{
					hardware_events_buffer[i][bufindex] = hardware_events[i][j];
							
				}
			}
			if(!checkempty){
				++bufindex;
				++num_sample;
			}
		}
		checkempty = 0;
		printf("Stopping K-LEB...\n# of Sample: %d\n", num_sample);

		/*  Log to file	*/
		FILE *logfp = fopen(argv[6], "a");
		if( logfp == NULL ){
			fprintf(stderr,"Error opening file: %s\n", strerror(errno));
		}
		else{
			fprintf(logfp, "%s,%s,%s,%s,INST,CPU_CLK_CYCLE,CPU_REF_CYCLE,CPU,PID\n", argv[1], argv[2], argv[3], argv[4]);
			for (j = 0; j < bufindex && !checkempty; ++j)
			{
				for (i = 0; i < (num_events + 1) && !checkempty; ++i)
				{
					if(j==0){
						fprintf(logfp, "%ld,", hardware_events_buffer[i][j]);
					}
					else if(hardware_events_buffer[4][j]==0 && hardware_events_buffer[5][j]==0 && hardware_events_buffer[6][j]==0 && hardware_events_buffer[7][j]==0 && hardware_events_buffer[8][j]==0){
						printf("Sample is empty\n ");
						checkempty = 1;
						break;
					}
					else{
						if(i < 7 && hardware_events_buffer[i][j] < hardware_events_buffer[i][j-1]){
							/* Handle overflow */
							temp_overflow[i] += 4294967295;			
						}
						if(i < 7){
							/* Counters value */
							fprintf(logfp, "%ld,", hardware_events_buffer[i][j] + temp_overflow[i]);
						}
						else
						{
							/* CPU & PID */
							fprintf(logfp, "%ld,", hardware_events_buffer[i][j]);
						}	
					}
				}
				fprintf(logfp, "\n");
			}
			fprintf(logfp, "\n");
			printf("Log Path: %s\n ", argv[6]);
		}
		fclose(logfp);
		close(fd);
	}
}
