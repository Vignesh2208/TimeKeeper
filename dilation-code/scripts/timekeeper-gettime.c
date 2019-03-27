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
    printf("Returns the current virtual time of a container\n");
    fprintf(stderr, "Usage: %s [-p pid] [-n name]\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int pid;
    char * lxcname;
    struct timeval later;
    struct tm tm;

    pid = 0;
    lxcname = NULL;
    while ((opt = getopt(argc, argv, "pnh")) != -1) {
        switch (opt) {
        case 'p':
          pid = atoi(argv[optind]);
          if (pid <= 0) {
	    printf("Invalid pid\n");
	    printUsage(argv[0]);
	    return -1;
          }
        break;
	case 'n':
	  lxcname = argv[optind];
          if (lxcname == NULL) {
	    printf("No name supplied\n");
	    printUsage(argv[0]);
	    return -1;
	  }
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

    if (pid != 0)
        gettimepid(pid, &later, NULL);
    else
        gettimename(lxcname, &later, NULL);

    localtime_r(&(later.tv_sec), &tm);
    printf("%d localtime: %d:%02d:%02d %ld\n",pid, tm.tm_hour, tm.tm_min, tm.tm_sec, later.tv_usec);

    return 0;
}
