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


#define SIG_END 44

int select_dialated(int nfds, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, struct timeval *timeout){

#ifdef __x86_64
	return syscall(316,nfds,read_fds,write_fds,except_fds,timeout);
	
#endif
	return syscall(353,nfds,read_fds,write_fds,except_fds,timeout);
	

}

/*
The original, unmodified gettimeofday() system call
*/
void gettimeofdayoriginal(struct timeval *tv, struct timezone *tz) {
#ifdef __x86_64
	syscall(314, tv, tz);
	return;
#endif
	syscall(351, tv, tz);
	return;
}

/*
Takes an integer (the time_dilation factor) and returns the time dilated process.
    ie: time_dilation = 2 means that when 4 seconds happens in real life,
		the process only thinks 2 seconds has happened
	time_dilation = 1/2 means that when 2 seconds happens in real life,
		 the process thinks 4 seconds actually happened
*/
int clone_time(unsigned long flags, double dilation, int should_freeze) {
	int pid;
        pid = syscall(SYS_clone, flags, NULL, NULL, NULL, NULL);
	dilate_all(pid, dilation);
	if (should_freeze == 1) {
		freeze_all(pid);
	}
	return 0;
}

/*
Returns the virtual time of an LXC, given it's pid
*/
int gettimepid(int pid, struct timeval *tv, struct timezone *tz) {
#ifdef __x86_64
	return syscall(315, pid, tv, tz);
#endif
	return syscall(352, pid, tv , tz);
}

/*
Returns the virtual time of an LXC, given it's name
*/
int gettimename(char *lxcname, struct timeval *tv, struct timezone *tz) {
	int pid;
	if (is_root()) {
	    pid = getpidfromname(lxcname);
            if (pid == -1) {
	         printf("LXC with name: %s not found\n",lxcname);
                 return -1;
	    }
	    return gettimepid(pid, tv, tz);
	}
	return -1;
}

//Given a frozen process specified by PID, will advance it's virtual time by interval (microseconds)
int leap(int pid, int interval) {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		if (interval > 0) {
			sprintf(command, "%c,%d,%d", LEAP, pid, interval);
			if (send_to_timekeeper(command) == -1)
				return -1;
			return 0;
		}
	}
	return -1;
}

/*
Given a pid, add that container to an experiment. If the timeline is less than 0, add to a CBE experiment
else, it represents a specific timeline
*/
int addToExp(int pid, int timeline) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
		if (timeline < 0) {
        	        sprintf(command, "%c,%d", ADD_TO_EXP_CBE, pid);
		}
		else {
			sprintf(command, "%c,%d,%d", ADD_TO_EXP_CS, pid, timeline);
		}
		if (send_to_timekeeper(command) == -1)
			return -1;
        	return 0;
	}
        return -1;
}

/*
Starts a CBE Experiment
*/
int startExp() {
	if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c", START_EXP);
		send_to_timekeeper(command);
                return 0;
        }
        return 1;
}

/*
Given all Pids added to experiment, will set all their virtual times to be the same, then freeze them all (CBE and CS)
*/
int synchronizeAndFreeze() {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c", SYNC_AND_FREEZE);
                if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}

/*
Set the interval in which a pid in a given timeline should advance (microsends) (CS)
*/
int setInterval(int pid, int interval, int timeline) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c,%d,%d,%d", SET_INTERVAL, pid, interval, timeline);
		if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}


/*
Progress all containers in the given timeline to advance by their specified intervals. The function will return when all containers
have done so. (CS)
force = 0, do not force LXC times
force = 1, force LXC times to be progress exactly 
*/
struct msghdr msg;
int progress(int timeline, int force) {
    struct sockaddr_nl src_addr;
    int sock_fd;
    fd_set readfds;
    struct timeval tv;
    int n, rv;

    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0)
        return -1;

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = syscall(SYS_gettid);

    bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));


    char command[100];
    sprintf(command, "%c,%d,%d,%d", PROGRESS, timeline, gettid(), force);
    if (send_to_timekeeper(command) == -1)
	return -1;

    FD_ZERO(&readfds);
    FD_SET(sock_fd, &readfds);

    n = sock_fd + 1;
    tv.tv_sec = 5; //set for 5 seconds currently, should probably change this
    tv.tv_usec = 0;
// I dont think a timeout should be necessary anymore.
    rv = select(n, &readfds, NULL, NULL, &tv);

    if (rv == -1) {
	perror("select error");
    }
  else if (rv == 0) {
	printf("Timeout occurred, fix timeline..\n");
	sprintf(command, "%c,%d", FIX_TIMELINE, timeline);
	send_to_timekeeper(command);
	sleep(1);
    }
    else {
        recvmsg(sock_fd, &msg, 0);
    }
    close(sock_fd);
	return 0;

}

/*
Reset all pre-specifed intervals for a given timeline (CS)
*/
int reset(int timeline) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c,%d", RESET, timeline);
                if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}

/*
Stop a running experiment (CBE or CS) **Do not call stopExp if you are waiting for a s3fProgress to return!!**
*/
int stopExp() {
	if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c", STOP_EXP);
		if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}

/*
Sets the TDF of the given pid
*/
int dilate(int pid, double dilation) {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		int dil;
		if ( (dil = fixDilation(dilation)) == -1) {
			return -1;
		}
		printf("Trying to create dilation %d from %f\n",dil, dilation);
		if (dil < 0) {
		sprintf(command, "%c,%d,1,%d", DILATE, pid, dil*-1);
		}
		else {
		sprintf(command, "%c,%d,%d", DILATE, pid, dil);
		}
		if (send_to_timekeeper(command) == -1)
			return -1;
		return 0;
	}
	return -1;
}


/*
Will set the TDF of a LXC and all of its children
*/
int dilate_all(int pid, double dilation) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
		int dil;
		if ( (dil = fixDilation(dilation)) == -1) {
			return -1;
		}
		printf("Trying to create dilation %d from %f\n",dil, dilation);
                if (dil < 0) {
                	sprintf(command, "%c,%d,1,%d", DILATE_ALL, pid, dil*-1);
                }
                else {
                	sprintf(command, "%c,%d,%d", DILATE_ALL, pid, dil);
                }
		if (send_to_timekeeper(command) == -1)
			return -1;
		return 0;
        }
        return -1;
}

/*
Takes an integer (pid of the process). This function will essentially 'freeze' the
time of the process. It does this by sending a sigstop signal to the process.
*/
int freeze(int pid) {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		sprintf(command, "%c,%d,%d", FREEZE_OR_UNFREEZE, pid, SIGSTOP);
		send_to_timekeeper(command);
		return 1;
	}
	return -1;
}

/*
Takes an integer (pid of the process). This function will unfreeze the process.
When the process is unfrozen, it will think that no time has passed, and will
continue doing whatever it was doing before it was frozen.
*/
int unfreeze(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c,%d,%d", FREEZE_OR_UNFREEZE, pid, SIGCONT);
		if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}

/*
Same as freeze, except that it will freeze the process as well as all of its children.
*/
int freeze_all(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c,%d,%d", FREEZE_OR_UNFREEZE_ALL, pid, SIGSTOP);
		if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}

/*
Same as unfreeze, except that it will unfreeze the process as well as all of its children.
*/
int unfreeze_all(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "%c,%d,%d", FREEZE_OR_UNFREEZE_ALL, pid, SIGCONT);
		if (send_to_timekeeper(command) == -1)
			return -1;
                return 0;
        }
        return -1;
}
