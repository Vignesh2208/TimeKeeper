#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>

#define DEF_MICROSEC	1000000

int main(int argc, char *argv[])
{
	uint64_t diff_sec, diff_nsec, micro_sec,diff_microsec;
	int k,def_cnt,deviation;
	const unsigned long long nano = 1000000000;
	unsigned long long start_nsec,end_nsec;
	useconds_t sleep_usec;
	struct timespec tp_start;
	struct timespec tp_end;
	char fmt[64], buf[64];
	unsigned long long t1,t2;

	def_cnt = 1;
	sleep_usec = DEF_MICROSEC;
	if( argc == 3 )
	{
		sleep_usec = atoi(argv[1]);
		def_cnt = atoi(argv[2]);
	}

	for(k = 0; k < def_cnt; k++)
	{
		if( clock_gettime( CLOCK_MONOTONIC, &tp_start) == -1 )
		{
			printf("Start Clock Gettime Failed\n");
			exit(1);
		}
		// printf("Sleeping for %d\n", sleep_usec);
		if(usleep( sleep_usec ) == -1)
		{
			printf("Usleep Error\n");
			exit(1);
		}
		if( clock_gettime( CLOCK_MONOTONIC, &tp_end) == -1 )
		{
			printf("End Clock Gettime Failed\n");
			exit(1);
		}


		start_nsec = tp_start.tv_sec * nano + tp_start.tv_nsec;
		end_nsec =  tp_end.tv_sec * nano + tp_end.tv_nsec;

		diff_nsec = (end_nsec - start_nsec);

		diff_sec = (( diff_nsec ) / nano );
		micro_sec = (diff_nsec / 1000);
		diff_microsec = (micro_sec - sleep_usec);
		deviation = (diff_microsec / sleep_usec )* 100;
		printf("Requested : %d MicroSecs <--> Elapsed Sec : %lu Elapsed Microsec :%lu Deviation : %lu\n", sleep_usec, diff_sec, micro_sec, diff_microsec);
		fflush(stdout);

	}
}
