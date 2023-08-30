/* Copyright (c) 2017, 2023 James Bruska, Caleb DeLaBruere, Chutitep Woralert

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
	else if (strcmp(event_name,"BR_IMMED_SPEC") == 0) return BR_IMMED_SPEC;
	else if (strcmp(event_name,"BR_RETURN_SPEC") == 0) return BR_RETURN_SPEC;
	else if (strcmp(event_name,"BR_INDIRECT_SPEC") == 0) return BR_INDIRECT_SPEC;
	/** Cache Events **/
	else if (strcmp(event_name,"LOAD") == 0) return LOAD;
	else if (strcmp(event_name,"STORE") == 0) return STORE;
	else if (strcmp(event_name,"L1D_CACHE_LD") == 0) return L1D_CACHE_LD;
	else if (strcmp(event_name,"L1D_CACHE_ST") == 0) return L1D_CACHE_ST;
	else if (strcmp(event_name,"L1D_CACHE_REFILL_LD") == 0) return L1D_CACHE_REFILL_LD;
	else if (strcmp(event_name,"L1D_CACHE_REFILL_ST") == 0) return L1D_CACHE_REFILL_ST;
	else if (strcmp(event_name,"L2D_CACHE_LD") == 0) return L2D_CACHE_LD;
	else if (strcmp(event_name,"L2D_CACHE_ST") == 0) return L2D_CACHE_ST;
	else if (strcmp(event_name,"L2D_CACHE_REFILL_LD") == 0) return L2D_CACHE_REFILL_LD;
	else if (strcmp(event_name,"L2D_CACHE_REFILL_ST") == 0) return L2D_CACHE_REFILL_ST;
	else if (strcmp(event_name,"L2D_CACHE") == 0) return L2D_CACHE;
	else if (strcmp(event_name,"L2D_CACHE_REFILL") == 0) return L2D_CACHE_REFILL;

	/* UNKNOWN Event */
	else return UNKNOWN_EVENT;
}

int main(int argc, char **argv)
{
	/* Check parameters */
	if(argc < 2){
		printf("Error reading configurations\nExiting...\n");
		exit(0);
	}

	int usrcmd;
	int checkempty;
	float hrtimer = 1;
	kleb_ioctl_args_t kleb_ioctl_args;
	int pid;
	char logpath[200];
	int num_events = 0;
	char eventname[20];
	int index;
	int mode = 0; //0 single; 1 all
	
	/* Open module */
	int fd = open(DEVICE_PATH, O_RDWR);
	if (fd == -1)
	{
		printf("Error in opening file \n");
		perror("Emesg");
		exit(-1);
	}
	
	/* Catch Ctrl^C signal */
	checkint=0;
	signal(SIGINT, sigintHandler);

	/* Set default parameters */
	memset(kleb_ioctl_args.counter, 0, sizeof(kleb_ioctl_args.counter));
	kleb_ioctl_args.delay_in_ns = 10000000;
	kleb_ioctl_args.user_os_rec = 1; //User:1 OS:2 Both:3
	memset(logpath, 0, sizeof(logpath));
	strcpy(logpath,"./Output.csv");
	
	/* Parse inputs */
	for (index = 1; index < argc; ++index){
		//printf("Argc: %d index: %d argv: %s\n",argc , index, argv[index]);
		if(argv[index][0] == '-'){
			if(argv[index][1] == 't'){
				++index;
				hrtimer = strtof(argv[index], NULL);
				kleb_ioctl_args.delay_in_ns = hrtimer*1000000;
			}
			if(argv[index][1] == 'o'){
				++index;
				strcpy(logpath,argv[index]);
				
			}
			if(argv[index][1] == 'm'){
				++index;
				kleb_ioctl_args.user_os_rec = strtol(argv[index], NULL, 10);
			}
			if(argv[index][1] == 'a'){
				mode = 1;
				printf("Set monitor all\n");
			}
			if(argv[index][1] == 'e'){
				++index;
				memset(eventname, 0, sizeof(eventname));
				for(int i = 0; i < strlen(argv[index]); ++i){
					
					if(argv[index][i] == ','){
						if(isalpha(eventname[0])){		
							kleb_ioctl_args.counter[num_events] = NameToRawConfigMask(eventname);
						}
						else{
							kleb_ioctl_args.counter[num_events] = strtol(eventname, NULL, 16);
						}
						//printf("EVT: %u %s %d\n", kleb_ioctl_args.counter[num_events], eventname, num_events);
						++num_events;
						memset(eventname, 0, sizeof(eventname));
					}
					else{
						strncat(eventname, &argv[index][i], 1);
						if(i == (strlen(argv[index])-1)){					
							if(isalpha(eventname[0])){		
								kleb_ioctl_args.counter[num_events] = NameToRawConfigMask(eventname);
							}
							else{
								kleb_ioctl_args.counter[num_events] = strtol(eventname, NULL, 16);
							}
							//printf("EVT: %u %s %d\n", kleb_ioctl_args.counter[num_events], eventname, num_events);
							memset(eventname, 0, sizeof(eventname));
							++num_events;
							kleb_ioctl_args.num_events = num_events;
						}
					}
				}
				
			}

		}
		else{
			break;
		}
	}
	if(num_events > 6){
		printf("This module only support monitoring up to 6 events\n");
		exit(0);
	}
	else if(num_events == 0){
		kleb_ioctl_args.counter[0] = strtol("12", NULL, 16);
		kleb_ioctl_args.counter[1] = strtol("10", NULL, 16);
		kleb_ioctl_args.counter[2] = strtol("70", NULL, 16);
		kleb_ioctl_args.counter[3] = strtol("71", NULL, 16);
		++num_events;
	}
	printf("Monitor mode: %d\n", mode);
	printf("Num Evt: %d\n", num_events);
	
	/* Program monitoring */
	if(mode == 0){
		pid = atoi(argv[index]);
		if(!kill(pid, 0) && pid != 0){
			/* User pass in program pid */
			if(pid == 0 || pid == 1){
				printf("Unable to find process %d\n", pid);
				exit(0);
			}
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
			char* cmdargv[argc];
			for (int i = index; i < argc; ++i){
				cmdargv[i-index] = argv[i];
			}
			cmdargv[argc-index]=NULL;
			execvp(argv[index], cmdargv);
			perror("Error: ");
			exit(0);
		}
		else{
			/* Set pid to monitor */
			kleb_ioctl_args.pid = pid;
			/* Start monitoring */
			if(ioctl(fd, IOCTL_START, &kleb_ioctl_args) < 0)
			{
				printf("ioctl failed and returned errno %s \n",strerror(errno));
			}
		}
	}
	else{
		/* Monitor system */
		pid = 1;
		kleb_ioctl_args.pid = pid;
		printf("Monitor all\n");
		if(ioctl(fd, IOCTL_START, &kleb_ioctl_args) < 0)
		{
			printf("ioctl failed and returned errno %s \n",strerror(errno));
		}
	}		

	/* Tapping parameters */
	int status = 0;
	struct timespec t1, t2;
	unsigned long long int tap_time;
	
	int i, j;
	int num_recordings = 500;
	int num_sample = 0;
	int num_tap = 0;
	int bufindex = 0;
	/* Buffer for tapping */
	unsigned int **hardware_events = malloc( (num_events+1)*sizeof(unsigned int *) );
	int size_of_message = num_recordings * (num_events+1) * sizeof(long int);

	/* Buffer for storing data */
	long int hardware_events_buffer[num_events+1][100000];
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
	/* Initialize buffer */
	hardware_events[0] = malloc( (num_events+1)*num_recordings*sizeof(unsigned int) );
	for ( i=0; i < (num_events+1); ++i ) {
		hardware_events[i] = *hardware_events + num_recordings * i;
	}

	/* Monitor program */
	if(mode == 0){		
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
					checkempty = 1;
				}
				else{
					checkempty = 0;
					++num_tap;
					for ( j=0; j < num_recordings && !checkempty; ++j ) {
						for ( i=0; i < (num_events+1) && !checkempty; ++i ) { 
							if(hardware_events[0][j]==-10)
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
	}
	else{
			/* Monitor system */
			printf("Monitoring HPC... \nPress Ctrl+C to exit\n");
			while (!checkint) {	
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
					checkempty = 1;
				}
				else{
					checkempty = 0;
					++num_tap;
					for ( j=0; j < num_recordings && !checkempty; ++j ) {
						for ( i=0; i < (num_events+1) && !checkempty; ++i ) { 
							if(hardware_events[0][j]==-10)
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
	printf("Extract last data...\n");
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
		printf("Data is empty\n ");
	}
	else{
		checkempty = 0;
	}
	printf("Arranging last data...\n");
	for (j = 0; j < num_recordings && !checkempty; ++j)
	{
		for (i = 0; i < (num_events + 1) && !checkempty; ++i)
		{
			if(hardware_events[0][j]==-10)
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
	printf("Finish Extract last data... \n");
	checkempty = 0;
	printf("Stopping K-LEB...\n# of Sample: %d\n", num_sample);

	/*  Log to file	*/
	FILE *logfp = fopen(logpath, "w");
	if( logfp == NULL ){
		fprintf(stderr,"Error opening file: %s\n", strerror(errno));
	}
	else{
		printf("Logging data...\n");
		fprintf(logfp, "CPU_CLK_CYCLE,");
		for(j = 0; j < (num_events +1); ++j){
			fprintf(logfp, "%ld,", kleb_ioctl_args.counter[j]);
			
		}
		//printf("Print Events %d\n", num_events);
		fprintf(logfp, "\n");
		for (j = 0; j < bufindex && !checkempty; ++j)
		{
			for (i = 0; i < (num_events + 1) && !checkempty; ++i)
			{
				if(j==0){
					fprintf(logfp, "%ld,", hardware_events_buffer[i][j]);
				}
				else if(hardware_events_buffer[0][j]==-10){
					printf("Sample is empty\n ");
					checkempty = 1;
					break;
				}
				else{
					if(i < 7){
						
						fprintf(logfp, "%ld,", hardware_events_buffer[i][j]);
					}
					else
					{
						
						fprintf(logfp, "%ld,", hardware_events_buffer[i][j]);
					}	
				}
			}
			fprintf(logfp, "\n");
		}
		fprintf(logfp, "\n");
		printf("Log Path: %s\n ", logpath);
	}
	fclose(logfp);
	close(fd);
	
}
