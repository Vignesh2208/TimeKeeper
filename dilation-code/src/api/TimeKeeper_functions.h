#ifndef __TK_FUNCTIONS
#define __TK_FUNCTIONS
#include <sys/time.h>
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
#include "utility_functions.h"

#define MAX_PAYLOAD 2000
#define NETLINK_USER 31



#define IFNAMESIZ 16
#define MAX_BUF_SIZ 2000



// General Functions **********************


typedef unsigned long u32;

// Synchronization Functions **********************

int addToExp(float relative_cpu_speed, u32 n_round_instructions);
int addToExp_sp(float relative_cpu_speed, u32 n_round_instructions, pid_t pid);
int synchronizeAndFreeze(int n_expected_tracers);
int update_tracer_params(int tracer_pid, float relative_cpu_speed,
                         u32 n_round_instructions);
int write_tracer_results(char * result);
int set_netdevice_owner(int tracer_pid, char * intf_name);
int gettimepid(int pid);

int startExp();
int stopExp();
int initializeExp(int exp_type);


int progress_n_rounds(int n_rounds);
int progress();
int hello();
int get_experiment_stats(ioctl_args * args);
int fire_timers();


#endif