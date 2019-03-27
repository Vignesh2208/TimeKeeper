#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <linux/sched.h>
#include <time.h>
#include <sys/time.h>
#include <TimeKeeper_functions.h>
#include "utility_functions.h"

void printUsage(char *name) {
    printf("Set the time interval of the LXC.\n");
    fprintf(stderr, "Usage: %s [-p Pid] [-n name] TIME(us) TIMELINE\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int pid;
    int time_interval;
    int timeline;
    char * lxcname;
    pid = -1;
    lxcname = NULL;
    while ((opt = getopt(argc, argv, "nph")) != -1) {
        switch (opt) {
        case 'h':
            printUsage(argv[0]);
            return -1;
        case 'n':
            lxcname = argv[optind];
            break;
        case 'p':
            pid = atoi(argv[optind]);
            break;
	default: /* '?' */
            printUsage(argv[0]);
	    return -1;
        }
    }

   if ((lxcname == NULL && pid == -1)) {
	printUsage(argv[0]);
        return -1;
    }

   sscanf(argv[optind +1],"%d",&time_interval);
   sscanf(argv[optind +2],"%d",&timeline);

   if (lxcname != NULL)
	pid = getpidfromname(lxcname);

//	printf("%d %d %d\n", pid, time_interval, timeline);
	setInterval(pid, time_interval, timeline);

   exit(EXIT_SUCCESS);

return 0;

}
