#include "utils.h"






/***
 Convert string to integer
***/
int str_to_i(char *s)
{
        int i,n;
        n = 0;
        for(i = 0; *(s+i) >= '0' && *(s+i) <= '9'; i++)
                n = 10*n + *(s+i) - '0';
        return n;
}

/***
Used when reading the input from a userland process -> the TimeKeeper. Will basically return the next number in a string
***/
int get_next_value (char *write_buffer)
{
        int i;
        for(i = 1; *(write_buffer+i) >= '0' && *(write_buffer+i) <= '9'; i++)
        {
                continue;
        }

        if(write_buffer[i] == '\0')
        	return -1;

        if(write_buffer[i+1] == '\0')
        	return -1;

        return (i + 1);
}


void print_tracee_list(llist * tracee_list) {

	llist_elem * head = tracee_list->head;
	tracee_entry * tracee;
	LOG("Active tracees: ");
	while(head != NULL) {
		tracee = (tracee_entry *) head->item;
		LOG("%d->",tracee->pid);
		head = head->next;
	}
	LOG("\n");
}


int flush_buffer(char * buf, int len) {
	int i;
	for(i = 0; i < len; i++){
		buf[i] = '\0';
	}
	return 0;
}


void print_curr_time(char * str) {

  char buffer[26];
  int millisec;
  struct tm* tm_info;
  struct timeval tv;

  flush_buffer(buffer,26);
  gettimeofday(&tv, NULL);

  millisec = (int) tv.tv_usec/1000.0; // Round to nearest millisec
  if (millisec>=1000) { // Allow for rounding up to nearest second
    millisec -=1000;
    tv.tv_sec++;
  }

  tm_info = localtime(&tv.tv_sec);

  strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
  LOG("%s.%03d >> %s\n", buffer, millisec, str);
  fflush(stdout);


}




int run_command(char * full_command_str, pid_t * child_pid) {

	char ** args;
	char * iter = full_command_str;

	int i = 0;
	int n_tokens = 0;
	int token_no = 0;
	int token_found = 0;
	int ret;
	pid_t child;
	

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(0, &set);

	while(full_command_str[i] != '\0'){
		
		if(i != 0 && full_command_str[i-1] != ' ' && full_command_str[i] == ' '){
			n_tokens ++;
		}
		i++;
	}

	args = malloc(sizeof(char *)*(n_tokens + 2));
	
	if(!args) {
		LOG("Malloc error in run_command\n");
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
        	LOG("fork() failed in run_command: %s.\n", strerror(errno));
        	exit(-1);
    	}

    	if (!child) {
        	prctl(PR_SET_DUMPABLE, (long)1);
        	prctl(PR_SET_PTRACER, (long)getppid());
		ptrace(PTRACE_TRACEME,0,NULL, NULL);
        	fflush(stdout);
        	fflush(stderr);
        	execvp(args[0], &args[0]);
		free(args);
        	exit(2);
    	}
		
	*child_pid = child;
	//param.sched_priority = 99;
	//ret = sched_setscheduler(child, SCHED_RR, &param);
	//if(ret == 0)
	//	printf("Priority set successfull\n");

	
	sched_setaffinity(child, sizeof(set), &set);
	sched_setaffinity((pid_t)getpid(), sizeof(set), &set);

	return 0;

}




#ifdef TEST
void get_next_command(int sockfd, struct sockaddr_nl* dst_addr, struct msghdr* msg, struct nlmsghdr* nlh, pid_t* pid, u32* n_insns){

	char * payload = NULL;
	int i = 0;
 	
	init_msg_buffer(nlh, dst_addr);
	recvmsg(sockfd, msg, 0);
	payload = NLMSG_DATA(nlh);

	if(payload != NULL){

		if(strcmp(payload, "-1 -1") == 0) {
			*pid = -1;
			*n_insns = -1;
			return;
		}

		while(payload[i] != '\0'){

			if(payload[i] == ' '){
				payload[i] = '\0';
				*pid = atoi(payload);
				*n_insns = atoi(payload + i + 1);
				break;
			}
			i = i + 1;
		}
	}
	else{
		LOG("No payload received\n");
	}

}
#endif

int get_next_command_tuple(char * buffer, int resume_ptr, pid_t * pid, u32 * n_insns){


	int i = resume_ptr;
	int nxt_idx = 0;
	if(strcmp(buffer + resume_ptr, "STOP") == 0 || resume_ptr < 0){
		*pid = -1;
		*n_insns = 0;
		return -1;
	}

	*pid = 0;
	*n_insns = 0;

	if(buffer[i] != '|' || buffer[i] == '\0')
		return -1;

	*pid = str_to_i(buffer + i + 1);

	nxt_idx = get_next_value(buffer + i);

	if(nxt_idx == -1){
		*pid = 0;
		*n_insns = 0;
		return -1;
	}
	else{
		nxt_idx = nxt_idx + i;
		*n_insns = str_to_i(buffer + nxt_idx);

		resume_ptr = nxt_idx + get_next_value(buffer + nxt_idx);
		resume_ptr = resume_ptr - 1;
		return resume_ptr;
	
	}

}



