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
    printf("Brings all containers to the same point in virtual time and freezes them\n");
    fprintf(stderr, "Usage: %s\n", name);
    return;
}

int main(int argc, char *argv[])
{
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

   synchronizeAndFreeze();

   /* Other code omitted */

   exit(EXIT_SUCCESS);

return 0;

}
