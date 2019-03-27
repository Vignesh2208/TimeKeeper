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

int main(int argc, char *argv[])
{
	struct timeval now;
	struct timeval later;
	struct timeval now1;
        struct timeval later1;
	struct tm tm;
	int x;
	int i;
	int loops;

	if (argc < 2) {
		printf("need to add number of loops\n");
		return 0;
	}

	loops = atoi(argv[1]);

	x = 0;
	while(x < loops) {
		gettimeofday(&now, NULL);
		gettimeofdayoriginal(&now1, NULL);
		for(i=0; i<1000000000;i++){
                }
		x++;
                gettimeofday(&later, NULL);
		gettimeofdayoriginal(&later1, NULL);
		localtime_r(&(later.tv_sec), &tm);
		printf("%d %d virtual time: %ld:%ld physical time: %ld:%ld localtime: %d:%02d:%02d %ld\n",x,getpid(),later.tv_sec-now.tv_sec,later.tv_usec-now.tv_usec,later1.tv_sec-now1.tv_sec,later1.tv_usec-now1.tv_usec,tm.tm_hour, tm.tm_min, tm.tm_sec, later.tv_usec);
	}

	printf("Computation Done\n");
	return 0;
}

