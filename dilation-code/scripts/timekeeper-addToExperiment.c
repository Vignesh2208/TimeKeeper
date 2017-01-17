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
#include <utility_functions.h>

void printUsage(char *name) {
    printf("Adds the container with given pid to the experiment, -s will specifiy if this process is associated with a timeline, -d option will specify a TDF for the container\n");
    fprintf(stderr, "Usage: %s [-t timeline] [-d TDF] [-p pid] [-n name]\n", name);
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    int pid;
    float dil;
    int timeline;
    char *lxcname;
    dil = -1.0;
    timeline = -1;
    lxcname = NULL;
    pid = -1;

    while ((opt = getopt(argc, argv, "tdnp")) != -1) {
        switch (opt) {
	case 't':
	    timeline = atoi(argv[optind]);
	    break;
	case 'd':
	dil = atof(argv[optind]);
	    break;
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

   if ( (lxcname == NULL && pid == -1) ) {
	printUsage(argv[0]);
        return -1;
    }

  if (lxcname != NULL)
	pid = getpidfromname(lxcname);


   if (dil != -1.0) { //need to set TDF
	if (dilate_all(pid,dil)) {
		printf("Error, most likely invalid TDF\n");
		return -1;
	}
   }

//  printf("Given name: %s, got pid %d, dilation %f, tl %d\n", lxcname, pid, dil, timeline);

     addToExp(pid, timeline);


   exit(EXIT_SUCCESS);

return 0;

}
