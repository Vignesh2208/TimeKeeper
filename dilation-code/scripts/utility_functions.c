#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "utility_functions.h"

const char *FILENAME = "/proc/dilation/status"; //where TimeKeeper LKM is reading commands

/*
Sends a specific command to the TimeKeeper Kernel Module. To communicate with the TLKM, you send messages to the location specified by FILENAME
*/
int send_to_timekeeper(char * cmd) {
    FILE *fp;
    fp = fopen(FILENAME, "a");
    if (fp == NULL) {
        //printf("Error communicating with TimeKeeper\n");
        return -1;
    }
    fprintf(fp, "%s,", cmd); //add comma to act as last character
    fclose(fp);
    return 0;
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

/*
Converts the double into a integer, makes computation easier in kernel land
ie .5 is converted to -2000, 2 to 2000
*/
int fixDilation(double dilation) {
        int dil;
        if (dilation < 0) {
                printf("Negative dilation does not make sense\n");
                return -1;
        }
        if (dilation < 1.0 && dilation > 0.0) {
                dil = (int)((1/dilation)*1000.0);
                dil = dil*-1;
        }
        else if (dilation == 1.0 || dilation == -1.0) {
                dil = 0;
        }
        else {
                dil = (int)(dilation*1000.0);
        }
        return dil;
}

/*
Given a LXC name, returns the Pid
*/
int getpidfromname(char *lxcname) {
        char command[500];
        char temp_file_name[100];
        FILE *fp;
        int mytid;
        int pid;
        #ifdef SYS_gettid
                mytid = syscall(SYS_gettid);
        #else
                printf("error getting tid\n");
                return -1;
        #endif
        sprintf(temp_file_name, "/tmp/%d.txt", mytid);
        sprintf(command, "lxc-info -n %s | grep pid | tr -s ' ' | cut -d ' ' -f 2 > %s", lxcname, temp_file_name);
        system(command);
        fp = fopen(temp_file_name, "r");
        fscanf(fp, "%d", &pid);
        fclose(fp);
        sprintf(command, "rm %s", temp_file_name);
        system(command);
        return pid;
}

