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
    printf("Dilates a process with given pid, -r option will dilate all children as well. \n A TDF of 2.0 means for every 1 second of system time, the process will only think .5 seconds has passed. \n A TDF of .5 means that for every 1 second of system time, the process will think 2 seconds have passed\n");
    fprintf(stderr, "Usage: %s [-r] [-p Pid] [-n name] TDF\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int recurse;
    int pid;
    float dil;
    char * lxcname;
    pid = -1;
    lxcname = NULL;
    recurse = 0;
    while ((opt = getopt(argc, argv, "rnph")) != -1) {
        switch (opt) {
        case 'r':
            recurse = 1;
            break;
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

   sscanf(argv[optind +1],"%f",&dil);

   if (lxcname != NULL) 
	pid = getpidfromname(lxcname);
	
   if (recurse == 0) { //do not recurse
	if (dilate(pid,dil)) {
		printf("Error, most likely invalid TDF\n");
		return -1;
	}
   }
   else { //recurse
	if (dilate_all(pid,dil)) {
		printf("Error, most likely invalid TDF\n");
		return -1;
	}
   }
  
   exit(EXIT_SUCCESS);

return 0;

}
