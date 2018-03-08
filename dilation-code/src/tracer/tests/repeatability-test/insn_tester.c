#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include "linkedlist.h"
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <sched.h>
#include <inttypes.h> /* for PRIu64 definition */
#include <stdint.h>   /* for uint64_t */ 
#include "libperf.h"  /* standard libperf include */
#include <poll.h>
#include <linux/perf_event.h>
#include "../../libperf/src/libperf.h"
#include <stdlib.h>
struct sched_param param;


#define NETLINK_USER 31
#define MAX_PAYLOAD 1024 /* maximum payload size*/
#define EBREAK_SYSCALL 35

#define TID_EXITED 0
#define TID_SYSCALL_ENTER 1
#define TID_SYSCALL_EXIT 2
#define TID_CLONE_EVT 3
#define TID_FORK_EVT 4
#define TID_STOPPED 5
#define TID_CONT 6
#define TID_SINGLESTEP_COMPLETE 7
#define TID_OTHER 8
#define TID_NOT_MONITORED 10

#define SUCCESS 1
#define FAIL 0
#define PTRACE_MULTISTEP 0x42f0
#define PTRACE_GET_REM_MULTISTEP 0x42f1
#define PTRACE_SET_REM_MULTISTEP 0x42f2

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

int llist_elem_comparer(int* elem1, int * elem2){

	if(*elem1 == *elem2)
		return 0;
	return 1;

}

void print_curr_time(char * str) {

  char buffer[26];
  int millisec;
  struct tm* tm_info;
  struct timeval tv;

  gettimeofday(&tv, NULL);

  millisec = (int) tv.tv_usec/1000.0; // Round to nearest millisec
  if (millisec>=1000) { // Allow for rounding up to nearest second
    millisec -=1000;
    tv.tv_sec++;
  }

  tm_info = localtime(&tv.tv_sec);

  strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
  fprintf(stderr, "%s.%03d >> %s\n", buffer, millisec, str);
  fflush(stdout);


}



int run_command(char * full_command_str, pid_t * child_pid) {

	char ** args;
	char * iter = full_command_str;
	int i = 0;
	int n_tokens = 0;
	int token_no = 0;
	int token_found = 0;
	pid_t child;
	int ret;

	while(full_command_str[i] != '\0'){
		
		if(i != 0 && full_command_str[i-1] != ' ' && full_command_str[i] == ' '){
			n_tokens ++;
		}
		i++;
	}

	args = malloc(sizeof(char *)*(n_tokens + 2));
	
	if(!args) {
		printf("Malloc error\n");
		exit(-1);
	}
	args[n_tokens + 1] = NULL;

	i = 0;
	
	while(full_command_str[i] != '\n'){
		if(i == 0){
			args[0] = full_command_str;
			token_no ++;
			token_found = 1;
		}
		else{
			if(full_command_str[i] == ' '){
				full_command_str[i] = '\0';
				token_found = 0;
			}
			else if(token_found == 0) {
				args[token_no] = full_command_str + i;
				token_no ++;
				token_found = 1;
			}
				
		}
                i++;
        }
	full_command_str[i] = NULL;

	
	child = fork();
    	if (child == (pid_t)-1) {
        	fprintf(stderr, "fork() failed: %s.\n", strerror(errno));
        	exit(-1);
    	}

    	if (!child) {
		ptrace(PTRACE_TRACEME,0,NULL, NULL);
        	fflush(stdout);
        	fflush(stderr);
        	execvp(args[0], &args[0]);
		free(args);
        	exit(2);
    	}
		
	*child_pid = child;
	param.sched_priority = 99;
	//ret = sched_setscheduler(child, SCHED_RR, &param);
	//if(ret == 0)
	//	printf("Priority set successfull\n");

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	sched_setaffinity(child, sizeof(set), &set);
	sched_setaffinity((pid_t)getpid(), sizeof(set), &set);

	return 0;

}



int flush_buffer(char * buf, int len) {
	int i;
	for(i = 0; i < len; i++){
		buf[i] = '\0';
	}
	return 0;
}


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


void get_next_command(int sockfd, struct sockaddr_nl * dst_addr, struct msghdr * msg, struct nlmsghdr * nlh, int * n_insns){

	char * payload = NULL;
 	init_msg_buffer(nlh, dst_addr);


	recvmsg(sockfd, msg, 0);
	
	payload = NLMSG_DATA(nlh);
	if(payload != NULL){
		int i = 0;

		if(strcmp(payload, "-1") == 0) {
			*n_insns = -1;
			return;
		}
		*n_insns = atoi(payload);
	}
	else{
		printf("No payload received\n");	
	}

}

void setup_all_traces(llist * active_tids) {

	llist_elem * head = active_tids->head;
	int * tid;
	int ret = 0;
	unsigned long status = 0;

	while(head != NULL) {
		tid = (int*) head->item;
		printf("Waiting for Child : %d\n", *tid);
		do {
			ret = waitpid((pid_t)*tid, &status, 0);
		}while((ret == (pid_t) -1 && errno == EINTR ));
	
		if(errno == ESRCH) {
			printf("Child is dead. Removing from active tids ...\n");
			llist_remove(active_tids, tid);

		}
		else{
			if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
				printf("Child stopped. Setting trace options \n");
				ptrace(PTRACE_SETOPTIONS, (pid_t)*tid, NULL, PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT | PTRACE_O_TRACEFORK);
				if(errno == ESRCH)
					printf("Error setting ptrace options\n");
			}

		}
		
		head = head->next;		
		
	}

}


void print_active_tids(llist * active_tids) {

	llist_elem * head = active_tids->head;
	int * tid;
	printf("Active tids: ");
	while(head != NULL) {
		tid = (int *) head->item;
		printf("%d->",*tid);
		head = head->next;
	}
	printf("\n");
}

unsigned long wait_for_ptrace_events(llist * active_tids, pid_t pid, unsigned long * new_pid, struct libperf_data * pd) {

	llist_elem * head = active_tids->head;
	unsigned long status;
	int ret;
	unsigned long n_ints;
	char buffer[100];
	struct user_regs_struct regs;
	
	
	flush_buffer(buffer, 100);
	errno = 0;
	do{
		ret = waitpid(pid, &status, 0);
	} while(ret == (pid_t) - 1 && errno == EINTR);

	if((pid_t)ret != pid){
		// Check if errno == SPECIAL SIGNAL HERE. If so, query the process structure to determine if this is a syscall enter. return TID_SYSCALL_ENTER
		if(errno == EBREAK_SYSCALL){
			printf("Waitpid: Breaking out. Process entered blocking syscall\n");
			return SUCCESS;
		}
		return FAIL;
	}

	

	if(errno == ESRCH) {
		printf("Wait pid returned ESRCH\n");
		return FAIL;
	}


	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
		// check for different types of events and determine whether this is a system call stop
		// detected fork or clone event. get new tid.
		if(status>>8 == (SIGTRAP | (PTRACE_EVENT_FORK<<8))) {
			ptrace(PTRACE_GETEVENTMSG, NULL, new_pid);
			printf("Detected new forked process with tid: %d. Setting trace options.\n", *new_pid);
			
			ptrace(PTRACE_SETOPTIONS, (pid_t)*new_pid, NULL, PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT | PTRACE_O_TRACEFORK);
			llist_append(active_tids, new_pid);
			return SUCCESS;	

		}
		else if(status >>8 == (SIGTRAP | PTRACE_EVENT_CLONE << 8)) {
		
			ptrace(PTRACE_GETEVENTMSG, NULL, new_pid);
                        printf("Detected new cloned thread with tid: %d. Setting trace options.\n", *new_pid);
			ptrace(PTRACE_SETOPTIONS, (pid_t)*new_pid, NULL, PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT | PTRACE_O_TRACEFORK);
		        llist_append(active_tids, new_pid);
                        return SUCCESS;
		}
		else if(status >>8 == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
			printf("Detected process exit for : %d\n", pid);
			llist_remove(active_tids, &pid);
			print_active_tids(active_tids);
			return SUCCESS;
		}
		else{

			uint64_t counter = libperf_readcounter(pd,LIBPERF_COUNT_HW_INSTRUCTIONS);/* obtain counter value */
			libperf_disablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS);/* disable HW counter */
			//fprintf(stdout, "N HW instructions counter read: %"PRIu64"\n", counter); /* printout */

          		libperf_finalize(pd, 0); /* log all counter values */
			//printf("Single step completed for Process : %d\n", pid);
			ret = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
			//printf("Rip: %lX\n\n", regs.rip);
			
			if(ret == -1){
				printf("ERROR in GETREGS.\n");
				return FAIL;
			}
			return regs.rip;

		}


	}



	if(WIFEXITED(status)){
		printf("Detected exit status is %d\n", WEXITSTATUS(status));
		llist_remove(active_tids, &pid);
                print_active_tids(active_tids);
                return SUCCESS;

	}

	return SUCCESS;
	

}

int run_commanded_process(llist * active_tids, pid_t pid, int n_instructions){


	int i = 0;
	unsigned long new_pid;
	int ret;
	int cont = 1, counter = 0;
	unsigned long n_insns = (unsigned long) n_instructions;
	struct libperf_data * pd;
	struct pollfd ufd;
	memset(&ufd, 0, sizeof(ufd));
	uint64_t counter_val; /* obtain counter value */
	int poll_count = 0;
	char buffer[100];
	int singlestepmode = 1;

	if(n_insns < 500)
		singlestepmode = 1;
	else
		singlestepmode = 0;

	flush_buffer(buffer, 100);

    	

	while(1) {
	

		if(singlestepmode) {
			pd = libperf_initialize((int)pid,0); /* init lib */
			libperf_enablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS); /* enable HW counter */
			libperf_enablecounter(pd, LIBPERF_COUNT_SW_CONTEXT_SWITCHES); /* enable CONTEXT SWITCH counter */
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (unsigned long *)&n_insns);
			ret = ptrace(PTRACE_MULTISTEP, pid, 0, (unsigned long *)&n_insns);
			//sprintf(buffer, "PTRACE RESUMING process. ret = %d, error_code = %d",ret,errno);
			//print_curr_time(buffer);		
		}
		else{

			n_insns = n_insns - 500;
			pd = libperf_initialize((int)pid,0); /* init lib */
			libperf_ioctlrefresh(pd, LIBPERF_COUNT_HW_INSTRUCTIONS, (uint64_t )n_insns);
			libperf_enablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS); /* enable HW counter */
			libperf_enablecounter(pd, LIBPERF_COUNT_SW_CONTEXT_SWITCHES); /* enable CONTEXT SWITCH counter */
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (unsigned long *)&n_insns);
			ret = ptrace(PTRACE_CONT, pid, 0, 0);
			//sprintf(buffer, "PTRACE RESUMING process. error_code = %d",errno);
			//print_curr_time(buffer);

		}
		
		if(ret == -1)
			return FAIL;
		if(errno == ESRCH){
			usleep(10000);
			if(kill(pid,0) == -1 && errno == ESRCH) {
				printf("PTRACE_RESUME ERROR. Child process is Dead. Breaking\n");
				llist_remove(active_tids,&pid);
				print_active_tids(active_tids);
				cont = 0;
				errno = 0;
				break;
			}
			errno = 0;
			continue;
		}
		return wait_for_ptrace_events(active_tids, pid, &new_pid, pd);
	}

	return FAIL;
	

}


int main(int argc, char * argv[]){

	char * cmd_file_path = NULL;
	FILE * fp;
	char * line;
	size_t read;
	size_t len = 0;
	int new_pid;
	int proc_to_control, n_insns;
	llist active_tids;
	pid_t cmd_pid[2];
	int n_recv_commands = 0;
	struct libperf_data * pd;
	int n_cmds = 0, n_max_insns = 100000;
	

	unsigned long ret_acc = 0;
	unsigned long ret1, ret2;

	srand(time(NULL));   // should only be called once


	
	llist_init(&active_tids);
	llist_set_equality_checker(&active_tids,llist_elem_comparer);

	printf("My Pid: %d\n", (pid_t)getpid());
 
	
	if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
	        fprintf(stderr, "\n");
        	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
        	fprintf(stderr, "       %s CMD_FILE_PATH\n", argv[0]);
        	fprintf(stderr, "\n");
        	fprintf(stderr, "This program executes all COMMANDs specified in the CMD file path in trace mode\n");
        	fprintf(stderr, "\n");
        	return 1;
    	}


	sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    	if (sock_fd < 0){
		printf("SOCKET Error\n");
        	return -1;

	}

    	memset(&src_addr, 0, sizeof(src_addr));
    	src_addr.nl_family = AF_NETLINK;
    	src_addr.nl_pid = getpid(); /* self pid */

	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

    	memset(&dest_addr, 0, sizeof(dest_addr));
    	memset(&dest_addr, 0, sizeof(dest_addr));
    	dest_addr.nl_family = AF_NETLINK;
    	dest_addr.nl_pid = 1234; /* For Linux Kernel */
   	dest_addr.nl_groups = 0; /* unicast */

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	if(!nlh) {
		printf("Message space allocation failure\n");
		exit(-1);
	}



	cmd_file_path = argv[1];
	fp = fopen(cmd_file_path,"r");
	if(fp == NULL) 
		exit(-1);

	while ((read = getline(&line, &len, fp)) != -1) {
        	printf("Running Command: %s", line);
		run_command(line, &cmd_pid[0]);
		llist_append(&active_tids, &cmd_pid[0]);

		run_command(line, &cmd_pid[1]);
		llist_append(&active_tids, &cmd_pid[1]);
    	}

	setup_all_traces(&active_tids);
	ret_acc = 0;
	while(n_cmds < 100) {

		
		//get_next_command(sock_fd, &dest_addr, &msg,  nlh, &n_insns);
		//if(n_insns == -1)
		//	break;


		n_insns = rand() % n_max_insns; 

		if(n_insns <= 0 )
			n_insns = 1;

		ret1 = run_commanded_process(&active_tids, cmd_pid[0], n_insns);
		ret2 = run_commanded_process(&active_tids, cmd_pid[1], n_insns);

		if(ret1 == 0 || ret2 == 0){
		  printf("TEST FAILED. Couldn't run command\n");
		  return 0;
		}

		if(ret1 != ret2) {
		  	printf("TEST FAILED. Command = %d, ret1  = %lX, ret2 = %lX\n", n_insns, ret1, ret2);
			return 0;
		}
		else{
			printf("COMMAND SUCCEEDED. Cmd = %d, Result = %lX\n", n_insns, ret1);
		}
		
		n_cmds ++;
		
	}

	printf("TEST SUCCESS.\n");
		
	return 0;

}
