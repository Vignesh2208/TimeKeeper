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
	pid_t ret;
	for(i = 0; i < n_tracers; i++){
		status = 0;
		/*if(kill((pid_t)tracer_pids[i],0) == -1)
			return FAIL;
		*/
		ret = waitpid((pid_t)tracer_pids[i], &status,WNOHANG);

		if(ret == (pid_t)tracer_pids[i] && WIFEXITED(status))
			return FAIL;
	}



	//All tracers alive
	return SUCCESS;
}


int main(int argc, char * argv[]){

    char cwd[MAX_BUF_SIZ];
    int tail = 0;
    int n_tracers = 0;
    int n_processes_per_tracer = 1;
    int i = 0;
    char cmd1[MAX_BUF_SIZ];
    char cmd2[MAX_BUF_SIZ];
    char *cmds_file[N_MAX_TRACERS];
    char * tracer_args[3];
    int j = 0;
    int child_pids[N_MAX_TRACERS];
    int ret;
    pid_t ret_val;
    int n_exited_tracers = 0;
    int status;
    int test_n_insns = 1000000;

    tail = strlen(argv[0]) -1;
    while(argv[0][tail] != '/'){
    	tail = tail - 1;
    }
    if(tail >= 0)
    	argv[0][tail] = '\0';

    flush_buffer(cmd1,MAX_BUF_SIZ);
    flush_buffer(cmd2,MAX_BUF_SIZ);
    flush_buffer(cwd,MAX_BUF_SIZ);




    if (getcwd(cwd, sizeof(cwd)) != NULL)
       fprintf(stdout, "Current working dir: %s\n", cwd);
    else
       perror("getcwd() error");


    if(argv[0][0] == '.')
      sprintf(cwd + strlen(cwd), "%s", argv[0] + 1);
    else
      sprintf(cwd + strlen(cwd), "/%s", argv[0]);
    printf("Script dir: %s\n", cwd);

    n_tracers = 2;

    if(n_tracers <= 0 || n_processes_per_tracer <= 0){
    		printf("Both arguments must be greater than 0");
    		exit(-1);
    }

    if(n_tracers > N_MAX_TRACERS){
    	printf("Max number of Tracers is 100. Resetting\n");
    	n_tracers = N_MAX_TRACERS;
    }

    sprintf(cmd1, "python %s/sockserver.py", cwd);
    sprintf(cmd2, "python %s/sockclient.py", cwd);

    //printf("Command name: %s\n", cmd);

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

        if(i == 0){
          fprintf(fp,"%s > /tmp/tracer%d.log\n",cmd1, i + 1);
          //fprintf(fp,"%s\n",cmd1);
        }
        else{
          fprintf(fp,"%s > /tmp/tracer%d.log\n",cmd2, i + 1);
          //fprintf(fp,"%s\n",cmd2);
        }
      }

      fclose(fp);

   }


    printf("Initializing Exp ...\n");

    ret = initializeExp();
    if(ret <= FAIL){
    		printf("Initialize Exp Failed !\n");
    		exit(-1);
    }

    printf("Initialized Exp ...\n");


    pid_t child;
    char tracer_id[5];
    char rel_cpu_speed[5];
    char n_round_insns[15];
    char create_spinner[5];


    for(i = 0 ; i < n_tracers; i++){

    		flush_buffer(tracer_id,5);
    		flush_buffer(rel_cpu_speed,5);
    		flush_buffer(n_round_insns,15);
    		flush_buffer(create_spinner,5);

    		sprintf(tracer_id,"%d",i);
    		sprintf(rel_cpu_speed,"%d",1);
    		sprintf(n_round_insns,"%d",test_n_insns);
    		sprintf(create_spinner, "%d", 0);

    		child = fork();
      	if (child == (pid_t)-1) {
          	printf("fork() failed in run_command: %s.\n", strerror(errno));
          	exit(-1);
      	}

      	if (!child) {
          	fflush(stdout);
          	fflush(stderr);


          	execl("/usr/bin/tracer", "/usr/bin/tracer", tracer_id, cmds_file[i], rel_cpu_speed, n_round_insns,create_spinner, (char *)NULL);
          	fflush(stdout);
          	fflush(stderr);
          	exit(2);
      	}
      	else{
      		child_pids[i] = child;
      	}
    }

    usleep(5000000);


    if(check_tracers_status(child_pids, n_tracers) == FAIL){
    		printf("Exiting due to Error. Some Tracers died!\n");
    		return -1;
    }

    printf("Synchronize and Freezing ... \n");
    fflush(stdout);
    usleep(1000000);

    while(synchronizeAndFreeze(n_tracers) <= FAIL){
    	printf("Sync and Freeze Failed. Retrying in 1 sec\n");
      fflush(stdout);
    	usleep(1000000);
    	//exit(-1);
    }

    printf("Synchronize and Freeze succeeded !\n");
    fflush(stdout);

    usleep(1000000);

    printf("Progress Experiment for 100 Rounds !\n");
    fflush(stdout);
    progress_n_rounds(2000);
    fflush(stdout);
    printf("Stopping Experiment ... \n");
    fflush(stdout);
    //while(1);
    usleep(2000000);
    stopExp();

    printf("Waiting for tracers to exit ...\n");

    while(n_exited_tracers < n_tracers){
     for(i = 0; i < n_tracers; i++){
    	status = 0;
    	if(child_pids[i]){
    		ret_val = waitpid((pid_t)child_pids[i], &status,0);

    		if(ret_val == (pid_t)child_pids[i] && WIFEXITED(status)){
    			child_pids[i] = 0;
    			n_exited_tracers ++;
    		}

    	}
    }
    }	



    printf("Test Succeeded !\n");

    return 0;

}
