#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include "TimeKeeper_functions.h"
#include "TimeKeeper_definitions.h"
#include "utility_functions.h"


#define IFNAMESIZ 16
#define MAX_BUF_SIZ 200

int hello(){
	printf("Hello there from shared lib\n");
	return 0;
}

/*
Given a pid, add that container to an experiment. If the timeline is less than 0, add to a CBE experiment
else, it represents a specific timeline
*/
int addToExp(float relative_cpu_speed, u32 n_round_instructions) {

	if(n_round_instructions <= 0 || relative_cpu_speed <= 0.0)
		return -1;

	int rel_cpu_speed = (int)(1000.0*relative_cpu_speed);

    if (is_root() && isModuleLoaded()) {
        char command[100];
        flush_buffer(command,100);
		sprintf(command, "%c,%d,%d", REGISTER_TRACER, rel_cpu_speed,(int)n_round_instructions);
		return send_to_timekeeper(command);
	}
    return -1;
}

/*
Starts a CBE Experiment
*/
int startExp() {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c", START_EXP);
		return send_to_timekeeper(command);
    }
    return -1;
}

int initializeExp() {

	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c", INITIALIZE_EXP);
		return send_to_timekeeper(command);
    }
    return -1;

}

/*
Given all Pids added to experiment, will set all their virtual times to be the same, then freeze them all (CBE and CS)
*/
int synchronizeAndFreeze(int n_expected_tracers) {

	if(n_expected_tracers <= 0)
		return -1;
	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c,%d", SYNC_AND_FREEZE,n_expected_tracers);
		return send_to_timekeeper(command);
	}
	return -1;
}


/*
Stop a running experiment (CBE or CS) **Do not call stopExp if you are waiting for a s3fProgress to return!!**
*/
int stopExp() {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c", STOP_EXP);
		return send_to_timekeeper(command);
	}
	return -1;
}


int update_tracer_params(int tracer_pid, float relative_cpu_speed, u32 n_round_instructions){
	if(tracer_pid <= 0 || relative_cpu_speed <= 0.0 || n_round_instructions == 0)
		return -1;

	int rel_cpu_speed = (int)(1000.0*relative_cpu_speed);


	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c,%d,%d,%d", UPDATE_TRACER_PARAMS,tracer_pid, rel_cpu_speed, (int)n_round_instructions);
		return send_to_timekeeper(command);
	}
	return -1;
}

int set_netdevice_owner(int tracer_pid, char * intf_name){
	if(tracer_pid <= 0 || intf_name == NULL || strlen(intf_name) > IFNAMESIZ)
		return -1;

	
	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c,%d,%s", SET_NETDEVICE_OWNER,tracer_pid, intf_name);
		return send_to_timekeeper(command);
	}

	return -1;

}
int gettimepid(int pid){

	if(pid <= 0)
		return -1;

	
	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c,%d", GETTIMEPID,pid);
		return send_to_timekeeper(command);
	}

	return -1;
}


int progress_n_rounds(int n_rounds){

	if(n_rounds <= 0)
		return -1;

	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c,%d", PROGRESS_N_ROUNDS,n_rounds);
		return send_to_timekeeper(command);
	}

	return -1;
}
int progress(){
	if (is_root() && isModuleLoaded()) {
		char command[100];
		flush_buffer(command,100);
		sprintf(command, "%c", PROGRESS);
		return send_to_timekeeper(command);
    }
    return -1;
}


int write_tracer_results(char * result){

	if(result == NULL || strlen(result) > MAX_BUF_SIZ)
		return -1;

	
	if (is_root() && isModuleLoaded()) {
		char command[MAX_BUF_SIZ];
		flush_buffer(command,MAX_BUF_SIZ);
		sprintf(command, "%c,%s", TRACER_RESULTS,result);
		return send_to_timekeeper(command);
	}

	return -1;
}