#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <linux/sched.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#define NSCLONEFLGS				\
  (						\
   SIGCHLD       |				\
   CLONE_NEWNS   |				\
   CLONE_NEWUTS  |				\
   CLONE_NEWIPC  |				\
   CLONE_NEWPID	 |				\
   CLONE_NEWNET					\
  )

#define MOUNT_SYS_MIN_VERSION "2.6.35"

int main(int argc, char *argv[])
{
char command[100];
srand(time(NULL));
int r;
	while (1) {
		r = (rand() % 101) +10;
		sprintf(command, "ping -c5 10.0.0.%d", r);
		//printf("%s\n",command);
		system(command);
		sleep(1);
	}

  return 0;
}

