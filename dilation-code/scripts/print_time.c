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
#include <sys/stat.h>
#include <sys/poll.h>

int main(int argc, char *argv[])
{
	struct timeval now;
	struct timeval later;
	struct timeval now1;
        struct timeval later1;
	struct tm localtm;
	struct tm origtm;
	int x;
	int i;
	int loops;
	int pid;
	unsigned long long int fact = 1;

	if(argc == 2)
		loops = atoi(argv[1]);
	else
		loops = 10000;

	/*
	for(i = 0; i < 2; i++){
		pid = fork();
		if(pid == 0){
			while(1){
				
			for(i = 1; i < 100000000; i++){
				if(fact == 0)
					fact = 1;
				fact = fact*i;
			 }
			}
			return 0;
		}
	}*/
	
	printf("Started ...\n");
	fflush(stdout);
	
	while(1){
		x = 0;
		while(x < loops) {
			gettimeofday(&now, NULL);
			gettimeofdayoriginal(&now1, NULL);
			for(i=0; i<100000000;i++){
		    }
			//usleep(1000000);		
			x++;
		    gettimeofday(&later, NULL);
			gettimeofdayoriginal(&later1, NULL);
			localtime_r(&(later.tv_sec), &localtm);
			localtime_r(&(later1.tv_sec),&origtm);
			//printf("%d %d virtual time: %ld:%ld physical time: %ld:%ld localtime: %d:%02d:%02d %ld\n",x,getpid(),later.tv_sec-now.tv_sec,later.tv_usec-now.tv_usec,later1.tv_sec-now1.tv_sec,later1.tv_usec-now1.tv_usec,localtm.tm_hour, localtm.tm_min, localtm.tm_sec, later.tv_usec);

			printf("localtime: %d:%02d:%02d %ld, orig_time : %d:%02d:%02d %ld\n", localtm.tm_hour, localtm.tm_min, localtm.tm_sec, later.tv_usec, origtm.tm_hour, origtm.tm_min, origtm.tm_sec, later1.tv_usec);
			fflush(stdout);
		}
	}
	printf("Computation Done\n");
	return 0;
}

