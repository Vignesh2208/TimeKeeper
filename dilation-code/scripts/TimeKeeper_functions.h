#include <sys/time.h>

#define MAX_PAYLOAD 1024
#define NETLINK_USER 31

// General Functions **********************

/*
Takes an integer (the time_dilation factor) and returns the time dilated process.
    ie: time_dilation = 2 means that when 4 seconds happens in real life,
                the process only thinks 2 seconds has happened
        time_dilation = 1/2 means that when 2 seconds happens in real life,
                 the process thinks 4 seconds actually happened
*/
int clone_time(unsigned long flags, double dilation, int should_freeze);

//Sets the TDF of the given pid
int dilate(int pid, double dilation);

//Will set the TDF of a pid and all of its children
int dilate_all(int pid, double dilation);

/*
Takes an integer (pid of the process). This function will essentially 'freeze' the
time of the process. It does this by sending a sigstop signal to the process.
*/
int freeze(int pid);

//Same as freeze, except that it will freeze the process as well as all of its children.
int freeze_all(int pid);

/*
Takes an integer (pid of the process). This function will unfreeze the process.
When the process is unfrozen, it will think that no time has passed, and will
continue doing whatever it was doing before it was frozen.
*/
int unfreeze(int pid);

//Same as unfreeze, except that it will unfreeze the process as well as all of its children.
int unfreeze_all(int pid);

//Gets the current virtual time of the LXC assoctiate with the lxcname
int gettimename(char *lxcname, struct timeval *tv, struct timezone *tz);

//Gets the current virtual time of the process associated via pid
int gettimepid(int pid, struct timeval *tv, struct timezone *tz);

//Just like gettimeofday() system call, but returns the actual system time
void gettimeofdayoriginal(struct timeval *tv, struct timezone *tz);

//Given a frozen process specified by PID, will advance it's virtual time by interval (microseconds)
int leap(int pid, int interval);

// Synchronization Functions **********************

/*
Given a pid, add that container to an experiment. If the timeline is less than 0, add to a CBE experiment
else, it represents a specific timeline in a CS experiment
*/
int addToExp(int pid, int timeline);

//Given all Pids added to experiment, will set all their virtual times to be the same, then freeze them all (CBE and CS)
int synchronizeAndFreeze();

//Starts a CBE Experiment
int startExp();

//Set the interval in which a pid in a given timeline should advance (microsends) (CS)
int setInterval(int pid, int interval, int timeline);

/*
Progress all containers in the given timeline to advance by their specified intervals. The function will return when all containers
have done so. (CS)
force = 0, do not force LXC times
force = 1, force LXC times to be progress exactly 
*/
int progress(int timeline, int force);

//Reset all pre-specifed intervals for a given timeline (CS)
int reset(int timeline);

//Stop a running experiment (CBE or CS) **Do not call stopExp if you are waiting for a progress() to return!!**
int stopExp();

