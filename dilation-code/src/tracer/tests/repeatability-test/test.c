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

  millisec = (int) tv.tv_usec / 1000.0; // Round to nearest millisec
  if (millisec >= 1000) { // Allow for rounding up to nearest second
    millisec -= 1000;
    tv.tv_sec++;
  }

  tm_info = localtime(&tv.tv_sec);

  strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
  fprintf(stderr, "%s.%03d\n", buffer, millisec);
  fflush(stdout);


}

unsigned long fibonacci(unsigned long curr, unsigned long prev) {

  return prev + curr;
}

void main() {
  int i = 1;
  unsigned long prev = 0, curr = 1;
  int delay = 1000;
  unsigned long ret = 1;
  while (i < 500000000) {
    ret = fibonacci(curr, prev);
    prev = curr;
    curr = ret;
    i++;
  }
}
