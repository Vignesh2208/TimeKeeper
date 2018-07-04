#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#define NETLINK_USER 31
#define MAX_PAYLOAD 1024 /* maximum payload size*/

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sockfd;
struct msghdr msg;

void init_msg_buffer(struct nlmsghdr *nlh, struct sockaddr_nl * dst_addr) {
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)dst_addr;
    msg.msg_namelen = sizeof(*dst_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
}


void send_command(int sockfd, struct msghdr * msg, struct nlmsghdr * nlh, int tracer_pid, int child_process_pid, int n_instructions){
	sprintf(NLMSG_DATA(nlh),"%d %d", child_process_pid, n_instructions);
    sendmsg(sockfd, msg, 0);
	printf("Sent command\n");
    exit(0);
}




int main(int argc, char * argv[]){

    int tracer_pid = 0;
    int child_process_pid = 0;
    int n_instructions = 0;
    int c;

    opterr = 0;
    while ((c = getopt (argc, argv, "t:c:n:h")) != -1){
        switch (c)
        {
            case 't': tracer_pid = atoi(optarg);
                      break;
            case 'c': child_process_pid = atoi(optarg);
                      break;
            case 'n': n_instructions = atoi(optarg);
                      break;
            case '?':
            case 'h': fprintf(stderr, "\n");
                      fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
                      fprintf(stderr, "       %s -t TRACER_PID -c CHILD_PROCESS_PID -n N_INSTRUCTIONS\n", argv[0]);
                      fprintf(stderr, "\n");
                      fprintf(stderr, "This program sends a command to PTRACE LOADER to advance a process specified by PROCESS PID by the specified number of instructions\n");
                      fprintf(stderr, "\n");
                      break;
        }
    }


    if (argc < 4 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
        fprintf(stderr, "       %s -t TRACER_PID -c CHILD_PROCESS_PID -n N_INSTRUCTIONS\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "This program sends a command to PTRACE LOADER to advance a process specified by PROCESS PID by the specified number of instructions\n");
        fprintf(stderr, "\n");
        return 1;
    }


	printf("Sending command to Tracer : %d, Process to control: %d, number of instructions: %d\n", tracer_pid, child_process_pid, n_instructions);


    sockfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    if (sockfd < 0){
        printf("SOCKET Error\n");
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); /* self pid */
    bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = tracer_pid; /* For Linux Kernel */
    dest_addr.nl_groups = 0; /* unicast */

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if(!nlh) {
        printf("Message space allocation failure\n");
        exit(-1);
    }
	init_msg_buffer(nlh, &dest_addr);
	send_command(sockfd, &msg,nlh, tracer_pid, child_process_pid, n_instructions); 

	return 0;
}

