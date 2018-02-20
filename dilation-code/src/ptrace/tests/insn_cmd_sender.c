#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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


void send_command(int sockfd, struct msghdr * msg, struct nlmsghdr * nlh, int ptrace_loader_pid, int n_instructions){

	sprintf(NLMSG_DATA(nlh),"%d", n_instructions);
        sendmsg(sockfd, msg, 0);
	printf("Sent command\n");
        exit(0);

}



int main(int argc, char * argv[]){

        int ptrace_loader_pid = 0;
	int process_pid = 0;
	int n_instructions = 0;



        if (argc < 3 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
                fprintf(stderr, "\n");
                fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
                fprintf(stderr, "       %s PTRACE_LOADER_PID N_INSTRUCTIONS\n", argv[0]);
                fprintf(stderr, "\n");
                fprintf(stderr, "This program sends a command to PTRACE LOADER to advance all processes under its control by the specified number of instructions\n");
                fprintf(stderr, "\n");
                return 1;
        }

	ptrace_loader_pid = atoi(argv[1]);
	n_instructions = atoi(argv[2]);

	printf("Ptrace loader: %d, number of instructions: %d\n", ptrace_loader_pid, n_instructions);


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
        dest_addr.nl_pid = ptrace_loader_pid; /* For Linux Kernel */
        dest_addr.nl_groups = 0; /* unicast */

        nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
        if(!nlh) {
                printf("Message space allocation failure\n");
                exit(-1);
        }
	init_msg_buffer(nlh, &dest_addr);
	send_command(sockfd, &msg,nlh, ptrace_loader_pid, n_instructions); 

	return 0;
}

