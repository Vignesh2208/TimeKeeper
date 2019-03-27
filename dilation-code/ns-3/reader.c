#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/poll.h>
#include <string.h>
#define MAX_BUF 1024

//This is just a directory to where you want the data to be stored

int main()
{
    int fd;
    char buf[MAX_BUF];
    char hostname[MAX_BUF];
    char debug[MAX_BUF];
    char command[MAX_BUF];
    gethostname(hostname,MAX_BUF);
    char myfifo[MAX_BUF];
    sprintf(myfifo, "/tmp/%s", hostname);
    mkfifo(myfifo, 0666);
    /* open, read, and display the message from the FIFO */
    fd = open(myfifo, O_RDWR | O_NONBLOCK );
    struct pollfd ufds;
    int result;
    int rv;
    int pid;
    int len;
    sprintf(debug, "echo Starting Debug for %s > /tmp/%s.output", hostname, hostname);
    system(debug);
    ufds.fd = fd;
    ufds.events = POLLIN;
while (1) {
    rv = poll(&ufds, 1, -1);
if (rv == -1) {
    perror("poll"); // error occurred in poll()
} else if (rv == 0) {
   printf("rv is 0\n");
 } else {
    // check for events on s1:
    if (ufds.revents & POLLIN) {
        result = read(fd, buf, MAX_BUF); // receive normal data
         if (strcmp(buf, "exit") == 0) {
	    printf("Exiting..\n");
	    break;
    	 }
    pid = fork();
    if (pid == 0) { //in child
    len = strlen(buf);
    sprintf(debug, "echo Running Command %s >> /tmp/%s.output", buf, hostname);
    if ( *(buf+len-1) == '\n') {
	*(buf+len-1) = '\0';
    }
    sprintf(command, "%s >> /tmp/%s.output 2>&1", buf, hostname);
//    sprintf(debug, "echo Running Command %s", buf);
//    sprintf(command, "%s", buf);
    //if last character in buf is a newline, remove it and continue.
    sprintf(debug, "echo Running Command %s >> /tmp/%s.output", buf, hostname);
    printf("%s\n", debug);
    sprintf(command, "%s >> /tmp/%s.output", buf, hostname);
    printf("%s\n", command);
    system(debug);
    system(command);
    return 0;
    }
    }
}

}
    close(fd);

    return 0;
}
