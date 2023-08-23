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
/* Log to file*/
int val_extract(unsigned int** hardware_events_buffer, int recording, int event,  FILE* log_path)
{
	int sample_count=0;
	int checkempty = 0;
	int i,j;
	for ( j=0; j < recording && !checkempty; ++j ) {
		for ( i=0; i < (event + 3) && !checkempty; ++i ) { 
			if(hardware_events_buffer[i][j]==-10)
			{
				/* End of data buffer */
				checkempty = 1;
				break;
			}
			else{
				fprintf(log_path, "%d,", hardware_events_buffer[i][j]);
			}
		}
		/* End of sample */
		if(!checkempty){
			++sample_count;
			fprintf(log_path, "\n");
		}
	}
	fflush(log_path);
	return sample_count;
}

int main(int argc, char **argv)
{

	int usrcmd;
	int checkempty;
	int index;
	kleb_ioctl_args_t kleb_ioctl_args;
	int pid;
	float hrtimer = 1;
	int mode = 0; //0 single; 1 all
	int num_events = 0;
	char eventname[20];
	char logpath[200];

	/* Check parameters */
	if(argc < 2){
		printf("Error reading configurations\nExiting...\n");
		exit(0);
	}

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
		if(argv[index][0] == '-'){
			if(argv[index][1] == 'a'){
				mode = 1;
				printf("Set monitor all\n");
			}
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
						//printf(", EVT: %u %s %d\n", kleb_ioctl_args.counter[num_events], eventname, num_events);
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

	/* Check number of event */
	if(num_events > 4){
				printf("This module only support monitoring up to 4 events\n");
				exit(0);
	}
	else if(num_events == 0){
		/* Set default event if none provided */
		kleb_ioctl_args.counter[0] = strtol("00c4", NULL, 16);
		kleb_ioctl_args.counter[1] = strtol("00c5", NULL, 16);
		kleb_ioctl_args.counter[2] = strtol("4f2e", NULL, 16);
		kleb_ioctl_args.counter[3] = strtol("412e", NULL, 16);
		num_events = 4;
		kleb_ioctl_args.num_events = num_events;
	}
	else{
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

			/* Execute child */
			if (pid == 0)
			{ 
				sleep(1);
				/* Phrase program arguments */
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
				/* Check if pid exist */
				//
				//printf("PID: %d \n", pid);
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
	}

	/* Tapping parameters */
	int status = 0;
	struct timespec t1, t2;
	unsigned long long int tap_time;
	/* Buffer parameters for tapping */
	int i, j;
	int num_recordings = 500;
	int num_sample = 0;
	unsigned int **hardware_events = malloc( (num_events+3)*sizeof(unsigned int *) );
	int size_of_message = num_recordings * (num_events+3) * sizeof(long int);
	/* Buffer for storing data */
	long int hardware_events_buffer[num_events+3][100000];
	

	/* Set user tapping time */
	tap_time = kleb_ioctl_args.delay_in_ns*60;
	if(tap_time >= 1000000000 ){
		t1.tv_sec = tap_time/1000000000;
		t1.tv_nsec = tap_time-(t1.tv_sec*1000000000);
	}
	else{
		t1.tv_sec = 0;
		t1.tv_nsec = tap_time;
	}

	/* Initialize buffer */
	hardware_events[0] = malloc( (num_events+3)*num_recordings*sizeof(unsigned int) );								
	for ( i=0; i < (num_events + 3); ++i ) {
		hardware_events[i] = *hardware_events + num_recordings * i;
	}

	/*  Log to file	event list */
	FILE *logfp = fopen(logpath, "w");
	if( logfp == NULL ){
		fprintf(stderr,"Error opening file: %s\n", strerror(errno));
	}
	else{
		printf("Logging data...\n");
		for(j = 0; j < (num_events + 3); ++j){
			if(j == num_events){
				fprintf(logfp, "INST_RETIRED,");
			}
			else if(j == num_events+1){
				fprintf(logfp, "CPU_CLK_CYCLE,");
			}
			else if(j == num_events+2){
				fprintf(logfp, "CPU_REF_CYCLE,");
			}
			else{
				fprintf(logfp, "%x,", kleb_ioctl_args.counter[j]);
			}
		}
		fprintf(logfp, "\n");
		printf("Log Path: %s\n ", logpath);
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
				}
				else{
					num_sample += val_extract(hardware_events, num_recordings, num_events, logfp);
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
				}
				else{
					num_sample += val_extract(hardware_events, num_recordings, num_events, logfp);
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
				}
				else{
					num_sample += val_extract(hardware_events, num_recordings, num_events, logfp);
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
		printf("Data is empty\n ");
	}
	else{
		num_sample += val_extract(hardware_events, num_recordings, num_events, logfp);
	}

	printf("Finish Extract last data... \n");
	printf("Stopping K-LEB...\n# of Sample: %d\n", num_sample);


	fclose(logfp);
	close(fd);
}
