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
    printf("Progresses LXCs in the specified timeline by their predetermined interval. -f flag to force containers to exact time you specified\n");
    fprintf(stderr, "Usage: %s [-f] [-t timeline]\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int timeline;
    int force;
    force = 0;

    while ((opt = getopt(argc, argv, "tfh")) != -1) {
        switch (opt) {
	case 't':
	    timeline = atoi(argv[optind]);
	    break;
	case 'f':
	    force = 1;
	    break;
        case 'h':
            printUsage(argv[0]);
            return -1;
        default: /* '?' */
            printUsage(argv[0]);
	    return -1;
        }
    }


   progress(timeline, force);
   exit(EXIT_SUCCESS);

return 0;

}
