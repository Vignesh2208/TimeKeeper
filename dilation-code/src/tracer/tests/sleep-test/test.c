#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

void print_curr_time() {

  char buffer[26];
  int millisec;
  struct tm* tm_info;
  struct timeval tv;

  gettimeofday(&tv, NULL);

  millisec = (int) tv.tv_usec/1000.0; // Round to nearest millisec
  if (millisec>=1000) { // Allow for rounding up to nearest second
    millisec -=1000;
    tv.tv_sec++;
  }

  tm_info = localtime(&tv.tv_sec);

  strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
  fprintf(stderr, "%s.%03d\n", buffer, millisec);
  fflush(stdout);


}

void main() {
	fprintf(stderr, "Hello World\n");
	fflush(stdout);
	int i = 0;
	int delay = 1000;
	fprintf(stderr, "Sleeping ...\n");
	usleep(delay*1000);
	print_curr_time();
	while(i < 50000000) {
		i++;
		/*if(i % 1000000 == 0){		
			break;
		}*/
	}
	print_curr_time();
	fprintf(stderr, "GoodBye World\n");

}
