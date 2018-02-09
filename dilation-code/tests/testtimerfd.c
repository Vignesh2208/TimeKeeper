
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define TIMEOUT 3
#define POLL_PERIOD 1
#define RECUR_TIMEOUT_SEC	1
#define RECUR_TIMEOUT_NSEC	0

int main(int argc, char *argv[])
{
	int ret;
	int fd = -1;
	struct itimerspec timeout;
	unsigned long long missed;
	time_t tv_sec, tv_nsec;
	time_t curtime;

	char fmt[64], buf[64];
	struct timeval tv;
	struct tm *t1;

	tv_sec = RECUR_TIMEOUT_SEC;
	tv_nsec = RECUR_TIMEOUT_NSEC; 
	if( argc == 3)
	{
		tv_sec = atol(argv[1]);
		tv_nsec = atol(argv[2]);
	}
    /* create new timer */
    fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd <= 0) {
        printf("Failed to create timer\n");
        return 1;
    }


    /* set timeout */
    timeout.it_value.tv_sec = TIMEOUT;
    timeout.it_value.tv_nsec = 0;
    timeout.it_interval.tv_sec = tv_sec; /* recurring */
    timeout.it_interval.tv_nsec = tv_nsec;
    ret = timerfd_settime(fd, 0, &timeout, NULL);
    if (ret) {
        printf("Failed to set timer duration\n");
        return 1;
    }

    while (1) {
        //printf("Polling\n");
        while (read(fd, &missed, sizeof(missed)) < 0) {
            // printf("No timer expiry\n");
            // sleep(POLL_PERIOD);
        }
        //printf("Number of expiries missed: %lld\n", missed);
	gettimeofday(&tv, NULL);
	if(( t1 = (struct tm *)localtime(&tv.tv_sec))!= NULL)
	{
		
		strftime(fmt, sizeof fmt, "%Y-%m-%d %H:%M:%S.%%06u ", t1);
		snprintf(buf, sizeof buf, fmt, tv.tv_usec);
		printf("%s\n", buf);
		fflush(stdout);
	}
    }

return 0;
}
