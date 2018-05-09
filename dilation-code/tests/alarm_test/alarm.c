#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/timerfd.h>

volatile sig_atomic_t print_flag = false;

void handle_alarm( int sig ) {
    print_flag = true;
}

int main() {

    char fmt[64], buf[64];
    struct timeval tv;
    struct tm *t1;
    signal( SIGALRM, handle_alarm ); // Install handler first,
    alarm( 1 ); // before scheduling it to be called.

    for (;;) {
        if ( print_flag ) {
            printf( "Hello\n" );
            gettimeofday(&tv, NULL);
            if(( t1 = (struct tm *)localtime(&tv.tv_sec))!= NULL)
            {
                
                strftime(fmt, sizeof fmt, "%Y-%m-%d %H:%M:%S.%%06u ", t1);
                snprintf(buf, sizeof buf, fmt, tv.tv_usec);
                printf("%s\n", buf);
                fflush(stdout);
            }
            print_flag = false;
            alarm( 1 );
        }
    }
}