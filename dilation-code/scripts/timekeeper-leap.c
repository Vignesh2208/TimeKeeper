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
    printf("Advance a process specified by interval (us)\n");
    fprintf(stderr, "Usage: %s <pid> <interval(us)>\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int pid;
    int time_interval;
    int opt;

    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
        case 'h':
            printUsage(argv[0]);
            return -1;
	default: /* '?' */
            printUsage(argv[0]);
	    return -1;
        }
    }

   if (argc != 3) {
   printUsage(argv[0]);
   return -1;
   }

   sscanf(argv[1],"%d",&pid);
   sscanf(argv[2],"%d",&time_interval);


	//printf("%d %d\n", pid, time_interval);
	leap(pid, time_interval);

   exit(EXIT_SUCCESS);

return 0;

}
