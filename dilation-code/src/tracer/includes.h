#ifndef __INCLUDES_H
#define __INCLUDES_H

#define _GNU_SOURCE

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
#include <stdio.h>
#include "linkedlist.h"
#include "hashmap.h"
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <sched.h>
#include <inttypes.h> /* for PRIu64 definition */
#include <stdint.h>   /* for uint64_t */
#include "libperf.h"  /* standard libperf include */
#include <poll.h>
#include <linux/perf_event.h>
#include "libperf/src/libperf.h"
#include <stdarg.h>
#include "TimeKeeper_functions.h"


#define NETLINK_USER 31
#define MAX_IGNORE_PIDS 100
#define EBREAK_SYSCALL 35
#define MAX_FNAME_SIZ 100

#define TID_EXITED 0
#define TID_SYSCALL_ENTER 1
#define TID_SYSCALL_EXIT 2
#define TID_CLONE_EVT 3
#define TID_FORK_EVT 4
#define TID_STOPPED 5
#define TID_CONT 6
#define TID_SINGLESTEP_COMPLETE 7
#define TID_OTHER 8
#define TID_NOT_MONITORED 10
#define TID_IGNORE_PROCESS 11

#define SUCCESS 1
#define FAIL -1
#define PTRACE_MULTISTEP 0x42f0
#define PTRACE_GET_REM_MULTISTEP 0x42f1
#define PTRACE_SET_REM_MULTISTEP 0x42f2
#define PTRACE_GET_MSTEP_FLAGS	0x42f3
#define WTRACE_DESCENDENTS	0x00100000


#define PTRACE_ENTER_SYSCALL_FLAG	1
#define PTRACE_BREAK_WAITPID_FLAG	2
#define PTRACE_ENTER_FORK_FLAG		3

#define INFO 0
#define WARNING 1
#define FATAL 2


#define   test_bit(_n,_p)     !! ( _n & ( 1u << _p))

typedef int pid_t;
typedef unsigned long u32;


typedef struct tracee_entry_struct {
	pid_t pid;
	int vfork_stop;
	int syscall_blocked;
	struct tracee_entry_struct * vfork_parent;

} tracee_entry;


void printLog(const char *fmt, ...);


#if defined DEBUG
#define LOG(...)    //printLog(__VA_ARGS__)
#else
#define LOG(...)	//printLog(__VA_ARGS__)
#endif



#endif
