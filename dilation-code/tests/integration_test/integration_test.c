#include <stdio.h>
#include "TimeKeeper_functions.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define MAX_BUF_SIZ 1024
#define N_MAX_TRACERS 100
#define SUCCESS 1
#define FAIL -1

void flush_buffer(char * buf, int size){
	int i;
	for(i = 0 ; i < size; i++)
		buf[i] = '\0';
}


int check_tracers_status(int * tracer_pids, int n_tracers){
	int i;
	int status = 0;
	for(i = 0; i < n_tracers; i++){
		status = 0;
		if(kill((pid_t)tracer_pids[i],0) == -1)
			return FAIL;
		waitpid((pid_t)tracer_pids[i], &status,WNOHANG);

		if(WIFEXITED(status))
			return FAIL;
	}



	//All tracers alive
	return SUCCESS;
}


int main(int argc, char * argv[]){

	char cwd[MAX_BUF_SIZ];
	int tail = 0;
	int n_tracers = 0;
	int n_processes_per_tracer = 0;
	int i = 0;
	char cmd[MAX_BUF_SIZ];
	char *cmds_file[N_MAX_TRACERS];
	char * tracer_args[3];
	int j = 0;
	int child_pids[N_MAX_TRACERS];

	if (argc < 3 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "\n");
    	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
    	fprintf(stderr,	"		%s NUMBER_OF_TRACERS NUM_PROCESSES_PER_TRACER\n", argv[0]);
    	fprintf(stderr, "\n");
    	fprintf(stderr, "This program spanws N tracers\n");
    	fprintf(stderr, "\n");
    	return 1;
    }

    tail = strlen(argv[0]) -1;
    while(argv[0][tail] != '/'){
    	tail = tail - 1;
    }
    if(tail >= 0)
    	argv[0][tail] = '\0';

    flush_buffer(cmd,MAX_BUF_SIZ);
    flush_buffer(cwd,MAX_BUF_SIZ);
    


    if (getcwd(cwd, sizeof(cwd)) != NULL)
       fprintf(stdout, "Current working dir: %s\n", cwd);
   	else
       perror("getcwd() error");


   sprintf(cwd + strlen(cwd), "%s", argv[0] + 1);
   printf("Script dir: %s\n", cwd);

   n_tracers = atoi(argv[1]);
   n_processes_per_tracer = atoi(argv[2]);

   if(n_tracers <= 0 || n_processes_per_tracer <= 0){
   		printf("Both arguments must be greater than 0");
   		exit(-1);
   }

   if(n_tracers > N_MAX_TRACERS){
   	printf("Max number of Tracers is 100. Resetting\n");
   	n_tracers = N_MAX_TRACERS;
   }

   sprintf(cmd, "%s/cmd\n", cwd);
   printf("Command name: %s", cmd);

   for(i = 0; i < n_tracers; i++){
 		cmds_file[i] = (char *)malloc(sizeof(char)*MAX_BUF_SIZ);
 		if(!cmds_file[i]){
 			printf("MALLOC ERROR\n");
 			exit(-1);
 		}
 		flush_buffer(cmds_file[i],MAX_BUF_SIZ);
 		sprintf(cmds_file[i], "%s/cmds_%d.txt", cwd,i);
 		printf("Commands file for Tracer: %d: %s\n", i, cmds_file[i]);

 		FILE * fp = fopen(cmds_file[i],"w+");
	    if(fp == NULL){
	   		printf("Error opening cmds file for Tracer: %d\n",i);
	   		exit(-1);
	   	}

	   	for(j = 0 ; j < n_processes_per_tracer; j++){
	   		//fwrite( cmd, 1 , sizeof(cmd) , fp );
	   		fprintf(fp,"%s",cmd);
	   	}

	    fclose(fp);

   }

   

   

   
   pid_t child;
   char tracer_id[5];
   char rel_cpu_speed[5];
   char n_round_insns[15];
   

   for(i = 0 ; i < n_tracers; i++){

   		flush_buffer(tracer_id,5);
   		flush_buffer(rel_cpu_speed,5);
   		flush_buffer(n_round_insns,15);

   		sprintf(tracer_id,"%d",i);
   		sprintf(rel_cpu_speed,"%d",1);
   		sprintf(n_round_insns,"%d",1000000);
   		child = fork();
    	if (child == (pid_t)-1) {
        	printf("fork() failed in run_command: %s.\n", strerror(errno));
        	exit(-1);
    	}

    	if (!child) {
        	fflush(stdout);
        	fflush(stderr);
        	execl("/usr/bin/tracer", "/usr/bin/tracer", tracer_id, cmds_file[i], rel_cpu_speed, n_round_insns,(char *)NULL);
        	fflush(stdout);
        	fflush(stderr);
        	exit(2);
    	}
    	else{
    		child_pids[i] = child;
    	}
   }

   usleep(2000000);

   
   if(check_tracers_status(child_pids, n_tracers) == FAIL){
   		printf("Exiting due to Error. Some Tracers died!\n");
   		return -1;
   }
   printf("Synchronize and Freezing ... \n");

   while(synchronizeAndFreeze(n_tracers) < 0){
   		usleep(10000);
   }

   printf("Synchronize and Freeze succeeded !\n");

   printf("Progress Experiment for 10 Rounds !\n");
   progress_n_rounds(10);

   printf("Stopping Experiment ... \n");
   stopExp();

   printf("Finishing Test ... \n");

   return 0;

}
