#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <sys/syscall.h>
#include "utility_functions.h"

//const char *FILENAME = "/proc/dilation/status"; //where TimeKeeper LKM is reading commands
const char *FILENAME = "/proc/status";
/*
Sends a specific command to the TimeKeeper Kernel Module. To communicate with the TLKM, you send messages to the location specified by FILENAME
*/
int send_to_timekeeper(char * cmd) {
    //FILE *fp;
    //fp = fopen(FILENAME, "a");
    int fp = open(FILENAME, O_RDWR);
    int ret = 0;
    if (fp < 0) {
        printf("Error communicating with TimeKeeper\n");
        return -1;
    }
    printf("Sending Command: %s\n", cmd);
    //ret = fprintf(fp, "%s,", cmd); //add comma to act as last character
    ret = write(fp, cmd, strlen(cmd));
    printf("Command Return value: %d\n", ret);
    close(fp);
    return ret;
}


/*
Returns the thread id of a process
*/
int gettid() {
        return syscall(SYS_gettid);
}

/*
Checks if it is being ran as root or not. All of my code requires root.
*/
int is_root() {
        if (geteuid() == 0)
                return 1;
        printf("Needs to be ran as root\n");
        return 0;

}

/*
Returns 1 if module is loaded, 0 otherwise
*/
int isModuleLoaded() {
    if( access( FILENAME, F_OK ) != -1 ) {
        return 1;
    } else {
        printf("TimeKeeper kernel module is not loaded\n");
        return 0;
    }
}

void flush_buffer(char * buf, int size){
    int i = 0;
    for(i =  0; i < size; i++)
        buf[i] = '\0';
}