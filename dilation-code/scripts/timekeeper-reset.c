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
    printf("Resets all LXCs interval within a specific timeline\n");
    fprintf(stderr, "Usage: %s timeline\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int timeline;

    while ((opt = getopt(argc, argv, "th")) != -1) {
        switch (opt) {
	case 't':
	    timeline = atoi(argv[optind]);
	    break;
        case 'h':
            printUsage(argv[0]);
            return -1;
        default: /* '?' */
            printUsage(argv[0]);
	    return -1;
        }
    }

   reset(timeline);

   /* Other code omitted */

   exit(EXIT_SUCCESS);

return 0;

}

