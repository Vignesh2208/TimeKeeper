#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <linux/sched.h>
#include <time.h>
#include "TimeKeeper_functions.h"
#include <sys/stat.h>
#include <sys/poll.h>
int input_timeout (int filedes, unsigned int seconds)
{
  fd_set set;
  struct timeval timeout;
  /* Initialize the file descriptor set. */
  FD_ZERO (&set);
  FD_SET (filedes, &set);

  /* Initialize the timeout data structure. */
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  printf("Calling select_dialated\n");
  return select_dialated(FD_SETSIZE,&set, NULL, NULL,&timeout);
  //return select_dialated(1,NULL, NULL, NULL,&timeout);
}
int main (void)
{

  struct timeval now;
  struct timeval later;
  struct timeval now1;
  struct timeval later1;
  struct tm localtm;
  struct tm origtm;
  int i;

  //for(i=0; i<1000000000;i++){
  //}

  //usleep(8000000);
  
  gettimeofday(&later, NULL);
  gettimeofdayoriginal(&later1, NULL);
  localtime_r(&(later.tv_sec), &localtm);
  localtime_r(&(later1.tv_sec),&origtm);
  printf("Before : localtime: %d:%02d:%02d %ld, orig_time : %d:%02d:%02d %ld\n", localtm.tm_hour, localtm.tm_min, localtm.tm_sec, later.tv_usec, origtm.tm_hour, origtm.tm_min, origtm.tm_sec, later1.tv_usec);
  fprintf (stdout, "select returned %d.\n", input_timeout (STDIN_FILENO, 5));
  gettimeofday(&later, NULL);
  gettimeofdayoriginal(&later1, NULL);
  localtime_r(&(later.tv_sec), &localtm);
  localtime_r(&(later1.tv_sec),&origtm);
  printf("After : localtime: %d:%02d:%02d %ld, orig_time : %d:%02d:%02d %ld\n", localtm.tm_hour, localtm.tm_min, localtm.tm_sec, later.tv_usec, origtm.tm_hour, origtm.tm_min, origtm.tm_sec, later1.tv_usec);
  fflush(stdout);

  return 0;
}