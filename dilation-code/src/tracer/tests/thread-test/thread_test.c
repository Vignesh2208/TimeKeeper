#include <pthread.h>
#include <stdio.h>
#define NUM_THREADS 1

 void *PrintHello(void *threadid)
 {
    long tid;
    int i = 0;
    int child;
    tid = (long)threadid;

//    child = fork();

//    if(child == 0) {
//	fprintf(stderr,"Hello World! It'me forked process inside thread #%ld!\n", tid);
//	exit(0);
//    }
	
    fprintf(stderr,"Hello World! It's me, thread #%ld!\n", tid);
    fflush(stdout);
    pthread_exit(NULL);
 }

 int main (int argc, char *argv[])
 {
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);

    for(t=0; t<NUM_THREADS; t++){
       printf("In main: creating thread %ld\n", t);
       rc = pthread_create(&threads[t], &attrs, PrintHello, (void *)t);
       if (rc){
          printf("ERROR; return code from pthread_create() is %d\n", rc);
          exit(-1);
       }
    }

    printf("In main: Before join\n");
    /* Last thing that main() should do */
    for(t=0; t<NUM_THREADS; t++) {
	pthread_join(threads[t], NULL);
    }

    printf("In main: Finishing\n");
 }

