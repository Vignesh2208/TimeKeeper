#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>

#define SYS_gettimeofdayreal 326

static struct timeval realtime;

gt(){ 


 syscall(SYS_gettimeofdayreal, &realtime, NULL);

}



int main() {
  
	char buf[64];
	time_t nowtime;

	gt();  



	nowtime = realtime.tv_sec;
	sprintf(buf,"%s", ctime(&nowtime));
	printf("%s",buf);
}

