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


void printUsage(char *name) {
    printf("Unfreezes a process that has previously been frozen, -r option will freeze all children as well. \n	Process will not percieve any passage of time\n");
    fprintf(stderr, "Usage: %s [-r] pid\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int recurse;
    int pid;

    recurse = 0;
    while ((opt = getopt(argc, argv, "rh")) != -1) {
        switch (opt) {
        case 'r':
            recurse = 1;
            break;
        case 'h':
            printUsage(argv[0]);
            return -1;
        default: /* '?' */
            printUsage(argv[0]);
	    return -1;
        }
    }
   if (optind >= argc) {
	printUsage(argv[0]);
        return -1;
    }

   pid = atoi(argv[optind]);
   if (pid <= 0) {
	printf("Invalid pid\n");
	printUsage(argv[0]);
	return -1;
   }

   if (recurse == 0) { //do not recurse
	unfreeze(pid);
   }
   else { //recurse
	unfreeze_all(pid);
   }
   
   /* Other code omitted */

   exit(EXIT_SUCCESS);

return 0;

}
