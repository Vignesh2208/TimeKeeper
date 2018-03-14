#include "tracer.h"
#include <sys/sysinfo.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#define TRACER_RESULTS 'J'
#define MAX_BUF_SIZ 1024

#ifdef TEST
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

#endif


struct sched_param param;
char logFile[MAX_FNAME_SIZ];

int llist_elem_comparer(tracee_entry * elem1, tracee_entry * elem2){

	if(elem1->pid == elem2->pid)
		return 0;
	return 1;

}


void printLog(const char *fmt, ...)
{
#ifdef DEBUG
  FILE* pFile = fopen(logFile, "a");
  if(pFile != NULL)
  {
   va_list args;
   va_start(args, fmt);
   vfprintf(pFile, fmt, args);
   va_end(args);
   fclose(pFile);
  }
#else

   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   fflush(stdout);
   va_end(args);

  
#endif
}

tracee_entry * alloc_new_tracee_entry(pid_t pid) {
	
	tracee_entry * tracee;

	
	tracee = (tracee_entry *) malloc(sizeof(tracee_entry));
	if(!tracee) {
		LOG("MALLOC ERROR !. Exiting for now\n");
		exit(-1);
	}

	tracee->pid = pid;
	tracee->vfork_stop = 0;
	tracee->syscall_blocked = 0;
	tracee->vfork_parent = NULL;
	
	return tracee;

}

void setup_all_traces(hashmap * tracees, llist * tracee_list) {

	llist_elem * head = tracee_list->head;
	tracee_entry * tracee;
	int ret = 0;
	unsigned long status = 0;

	while(head != NULL) {
		tracee = (tracee_entry *) head->item;
		do {
			ret = waitpid(tracee->pid, &status, 0);
		}while((ret == (pid_t) -1 && errno == EINTR ));
	
		if(errno == ESRCH) {
			LOG("Child %d is dead. Removing from active tids ...\n", tracee->pid);
			hmap_remove_abs(tracees, tracee->pid);
			llist_remove(tracee_list, tracee);

		}
		else{
			if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
				LOG("Child %d stopped. Setting trace options \n", tracee->pid);
				ptrace(PTRACE_SETOPTIONS, tracee->pid, NULL, PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |  PTRACE_O_TRACEEXEC);
				if(errno == ESRCH)
					LOG("Error setting ptrace options\n");
			}

		}
		
		head = head->next;		
		
	}

}


int wait_for_ptrace_events(hashmap * tracees, llist * tracee_list, pid_t pid, struct libperf_data * pd) {

	llist_elem * head = tracee_list->head;
	tracee_entry * new_tracee;
	tracee_entry * curr_tracee;
	struct user_regs_struct regs;
	
	int ret;
	u32 n_ints;
	u32 status = 0;
	uint64_t counter;

	char buffer[100];
	pid_t new_child_pid;
	

	curr_tracee = hmap_get_abs(tracees, pid);
	if(curr_tracee->vfork_stop)	//currently inside a vfork stop
		return TID_FORK_EVT;

	flush_buffer(buffer, 100);
	errno = 0;
	do{
		#ifdef DEBUG_VERBOSE
		print_curr_time("Entering waitpid");
		ret = waitpid(pid, &status, WTRACE_DESCENDENTS | __WALL);
		sprintf(buffer, "Ret waitpid = %d, errno = %d", ret, errno);
		print_curr_time(buffer);
		#else
		ret = waitpid(pid, &status, WTRACE_DESCENDENTS | __WALL);
		#endif

	} while(ret == (pid_t) - 1 && errno == EINTR);

	if((pid_t)ret != pid){
		if(errno == EBREAK_SYSCALL){
			LOG("Waitpid: Breaking out. Process entered blocking syscall\n");
			curr_tracee->syscall_blocked = 1;
			return TID_SYSCALL_ENTER;
		}
		return TID_IGNORE_PROCESS;
	}

	

	if(errno == ESRCH) {
		LOG("Process does not exist\n");
		return TID_IGNORE_PROCESS;
	}


	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {


		// check for different types of events and determine whether this is a system call stop
		// detected fork or clone event. get new tid.
		if(status >>8 == (SIGTRAP | PTRACE_EVENT_CLONE << 8) || status >>8 == (SIGTRAP | PTRACE_EVENT_FORK << 8)) {
		
			
			ret = ptrace(PTRACE_GETEVENTMSG,pid, NULL, (u32*)&new_child_pid);
			status = 0;
			do{
				ret = waitpid(new_child_pid, &status, __WALL);
			} while(ret == (pid_t) - 1 && errno == EINTR);

			if(errno == ESRCH) {
				LOG("Process does not exist\n");
				return TID_IGNORE_PROCESS;
			}

			if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
				new_tracee = alloc_new_tracee_entry(new_child_pid);
				llist_append(tracee_list, new_tracee);
				hmap_put_abs(tracees, new_child_pid, new_tracee);
				LOG("Detected new cloned child with tid: %d. status = %lX, Ret = %d, errno = %d, Set trace options.\n",new_child_pid, status, ret, errno);
				return TID_FORK_EVT;
			}
			else {
				LOG("ERROR ATTACHING TO New Thread status = %lX, ret = %d, errno = %d\n", status, ret, errno);
				return TID_IGNORE_PROCESS;
			}
			
			
                        
		}
		else if(status >>8 == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
			LOG("Detected process exit for : %d\n", pid);
			if(curr_tracee->vfork_parent != NULL) {
				curr_tracee->vfork_parent->vfork_stop = 0;
				curr_tracee->vfork_parent = NULL;
			}			
			return TID_EXITED;
		}
		else if(status >>8 == (SIGTRAP | PTRACE_EVENT_VFORK << 8)){
			LOG("Detected PTRACE EVENT FORK for : %d\n", pid);
			ret = ptrace(PTRACE_GETEVENTMSG,pid, NULL, (u32*)&new_child_pid);        
			status = 0;
			do{
				ret = waitpid(new_child_pid, &status, __WALL);
			} while(ret == (pid_t) - 1 && errno == EINTR);

			if(errno == ESRCH) {
				return TID_IGNORE_PROCESS;
			}

			if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP){
				new_tracee = alloc_new_tracee_entry(new_child_pid);
				llist_append(tracee_list, new_tracee);
				hmap_put_abs(tracees, new_child_pid, new_tracee);
				LOG("Detected new vforked process with pid: %d. status = %lX, Ret = %d, errno = %d, Set trace options.\n",new_child_pid, status, ret, errno);
				curr_tracee->vfork_stop = 1;
				new_tracee->vfork_parent = curr_tracee;
				return TID_FORK_EVT;
			}
			else {
				LOG("ERROR ATTACHING TO New Thread status = %lX, ret = %d, errno = %d\n", status, ret, errno);
				return TID_IGNORE_PROCESS;
			}
			
		}
		else if(status >>8 == (SIGTRAP | PTRACE_EVENT_EXEC << 8)){
			LOG("Detected PTRACE EVENT EXEC for : %d\n", pid);
			if(curr_tracee->vfork_parent != NULL){
				curr_tracee->vfork_parent->vfork_stop = 0;
				curr_tracee->vfork_parent = NULL;
			}
			return TID_OTHER;	
		}
		else{

			//counter = libperf_readcounter(pd,LIBPERF_COUNT_HW_INSTRUCTIONS);

			ret = ptrace(PTRACE_GET_REM_MULTISTEP, pid, 0, (u32*)&n_ints);
			//printf("n interrupts = %lu\n", n_ints);
			
      		libperf_disablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS);			
      		//fprintf(stdout, "N HW instructions counter read: %"PRIu64"\n", counter); 

		 	//counter = libperf_readcounter(pd,LIBPERF_COUNT_SW_CONTEXT_SWITCHES);
			//fprintf(stdout, "N CONTEXT SWITCHES counter read: %"PRIu64"\n", counter); 

      		//libperf_close(pd);
          	libperf_finalize(pd, 0); 	
			//ret = ptrace(PTRACE_GETREGS, pid, NULL, &regs);

			#ifdef DEBUG_VERBOSE
			LOG("Single step completed for Process : %d. Status = %lX. Rip: %lX\n\n ", pid, status, regs.rip);
			#endif

			if(ret == -1){
				LOG("ERROR in GETREGS.\n");
			}
			return TID_SINGLESTEP_COMPLETE;

		}


	}


	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGCHLD) {
		LOG("Received SIGCHLD. Process exiting...\n");
		if(curr_tracee->vfork_parent != NULL){
			curr_tracee->vfork_parent->vfork_stop = 0;
			curr_tracee->vfork_parent = NULL;
		}
		return TID_EXITED;
	}
	

	return TID_OTHER;
	

}

int run_commanded_process(hashmap * tracees, llist * tracee_list, pid_t pid, u32 n_insns, int cpu_assigned){


	int i = 0;
	int ret;
	int cont = 1;
	char buffer[100];
	int singlestepmode = 1;
	tracee_entry * curr_tracee;
	struct libperf_data * pd;

	u32 flags = 0;
	
	if(n_insns <= 1000)
		singlestepmode = 1;
	else
		singlestepmode = 0;


	curr_tracee = hmap_get_abs(tracees, pid);
	if(!curr_tracee)
		return FAIL;

	flush_buffer(buffer, 100);    	
	while(cont == 1) {
	
		if(curr_tracee->syscall_blocked){
			errno = 0;
			//check whether process is still syscall_blocked before attempting ptrace stepping
			ret = ptrace(PTRACE_GET_MSTEP_FLAGS, pid, 0, (u32*)&flags);
			if(test_bit(flags, PTRACE_ENTER_SYSCALL_FLAG) == 0){
				curr_tracee->syscall_blocked = 0;
				LOG("Process: %d , ret = %d, errno = %d, flags = %lX\n", pid, ret, errno, flags);
			}
			else {
				LOG("Process: %d is still blocked inside syscall\n",pid);
				return SUCCESS;
			}
	
		}
		else if(curr_tracee->vfork_stop){
			LOG("Process: %d is still blocked inside vfork call\n", pid);
			return SUCCESS;
		}

		if(singlestepmode) {
			errno = 0;
			pd = libperf_initialize((int)pid,cpu_assigned); /* init lib */
			libperf_enablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS); /* enable HW counter */
			//libperf_enablecounter(pd, LIBPERF_COUNT_SW_CONTEXT_SWITCHES); /* enable CONTEXT SWITCH counter */
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (u32*)&n_insns);

			#ifdef DEBUG_VERBOSE
			sprintf(buffer, "PTRACE RESUMING MULTI-STEPPING OF process. ret = %d, error_code = %d",ret,errno);
			print_curr_time(buffer);	
			#endif

			ret = ptrace(PTRACE_MULTISTEP, pid, 0, (u32*)&n_insns);
				
		}
		else{
			errno = 0;
			n_insns = n_insns - 500;
			pd = libperf_initialize((int)pid,cpu_assigned); /* init lib */
			libperf_ioctlrefresh(pd, LIBPERF_COUNT_HW_INSTRUCTIONS, (uint64_t )n_insns);
			libperf_enablecounter(pd, LIBPERF_COUNT_HW_INSTRUCTIONS); /* enable HW counter */
			//libperf_enablecounter(pd, LIBPERF_COUNT_SW_CONTEXT_SWITCHES); /* enable CONTEXT SWITCH counter */
			ret = ptrace(PTRACE_SET_REM_MULTISTEP, pid, 0, (u32*)&n_insns);

			#ifdef DEBUG_VERBOSE
			sprintf(buffer, "PTRACE RESUMING process. ret = %d, error_code = %d. pid = %d",ret, errno, pid);
			print_curr_time(buffer);
			#endif

			ret = ptrace(PTRACE_CONT, pid, 0, 0);
			

		}
		
		if(errno == ESRCH){
			usleep(10000);
			if(kill(pid,0) == -1 && errno == ESRCH) {
				LOG("PTRACE_RESUME ERROR. Child process is Dead. Breaking\n");
				if(curr_tracee->vfork_parent != NULL) {
					curr_tracee->vfork_parent->vfork_stop = 0;
					curr_tracee->vfork_parent = NULL;
				}
				llist_remove(tracee_list,curr_tracee);
				hmap_remove_abs(tracees, pid);
				print_tracee_list(tracee_list);
				
				cont = 0;
				errno = 0;
				return SUCCESS;
			}
			errno = 0;
			continue;
		}


		ret = wait_for_ptrace_events(tracees, tracee_list, pid, pd);
		

		switch(ret) {

			case TID_SYSCALL_ENTER:		curr_tracee->syscall_blocked = 1;
							return SUCCESS;
							break;

			case TID_IGNORE_PROCESS:	llist_remove(tracee_list, curr_tracee);
							hmap_remove_abs(tracees, pid);
							print_tracee_list(tracee_list);
							ptrace(PTRACE_CONT, pid, 0, 0);  // For now, we handle this case like this.
							return FAIL;
	
	
			case TID_EXITED:llist_remove(tracee_list, curr_tracee);
							hmap_remove_abs(tracees, pid);
							print_tracee_list(tracee_list);
							ptrace(PTRACE_CONT, pid, 0, 0); // Exit is still not fully complete. need to do this to complete it.
							return SUCCESS;
							break;

			default:			return SUCCESS;	

		}
		break;
	}

	
	
	return FAIL;
	

}

#ifdef TEST
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
#endif



int main(int argc, char * argv[]){

	char * cmd_file_path = NULL;
	int fp;
	char * line;
	size_t line_read;
	size_t len = 0;
	
	u32 n_insns;
	u32 n_recv_commands = 0;

	llist tracee_list;
	hashmap tracees;
	tracee_entry * new_entry;
	
	pid_t new_cmd_pid;
	pid_t new_pid;
	char * nxt_cmd[MAX_PAYLOAD];
	int tail_ptr;
	int tracer_id = 0;
	int count = 0;
	float rel_cpu_speed;
	u32 n_round_insns;
	int cpu_assigned;
	char command[MAX_BUF_SIZ];
	int n_cpus = get_nprocs();
	int read_ret = -1;

	

	hmap_init(&tracees, 1000);
	llist_init(&tracee_list);
	llist_set_equality_checker(&tracee_list,llist_elem_comparer);
 
	
	if (argc < 5 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
	        fprintf(stderr, "\n");
        	fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
        	fprintf(stderr,	"		%s TRACER_ID CMDS_FILE_PATH RELATIVE_CPU_SPEED N_ROUND_INSTRUCTIONS\n", argv[0]);
        	fprintf(stderr, "\n");
        	fprintf(stderr, "This program executes all COMMANDs specified in the CMD file path in trace mode\n");
        	fprintf(stderr, "\n");
        	return 1;
    	}


    tracer_id = atoi(argv[1]);
	cmd_file_path = argv[2];
	rel_cpu_speed = atof(argv[3]);
	n_round_insns = (u32) atoi(argv[4]);


	#ifdef DEBUG
	flush_buffer(logFile, MAX_FNAME_SIZ);
	sprintf(logFile,"/tmp/tracer%d.log",tracer_id);

	FILE* pFile = fopen(logFile, "w+");
	fclose(pFile);

	#endif


	LOG("Tracer Pid: %d, Tracer ID: %d, CMDS_FILE_PATH: %s, REL_CPU_SPEED: %f, N_ROUND_INSNS: %lu, N_CPUS: %d\n", (pid_t)getpid(), tracer_id, cmd_file_path, rel_cpu_speed, n_round_insns, n_cpus);

	fp = fopen(cmd_file_path,"r");
	if(fp == NULL) 
		exit(-1);

	while ((line_read = getline(&line, &len, fp)) != -1) {
		count ++;
       	LOG("Running Command no: %d: %s", count, line);
		run_command(line, &new_cmd_pid);
		new_entry = alloc_new_tracee_entry(new_cmd_pid);
		llist_append(&tracee_list, new_entry);
		hmap_put_abs(&tracees, new_cmd_pid, new_entry);
    }

	setup_all_traces(&tracees, &tracee_list);



	#ifdef TEST 
		sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
		if (sock_fd < 0){
			LOG("SOCKET Error\n");
	    	exit(-1);
		}


		memset(&src_addr, 0, sizeof(src_addr));
		src_addr.nl_family = AF_NETLINK;
		src_addr.nl_pid = getpid(); /* self pid */

		bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

		memset(&dest_addr, 0, sizeof(dest_addr));
		dest_addr.nl_family = AF_NETLINK;
		dest_addr.nl_pid = 1234; /* For Linux Kernel */
	   	dest_addr.nl_groups = 0; /* unicast */

		nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
		if(!nlh) {
			LOG("Message space allocation failure\n");
			exit(-1);
		}

		while(1) {
			get_next_command(sock_fd, &dest_addr, &msg,  nlh, &new_cmd_pid, &n_insns);
			if(new_cmd_pid == -1 )
				break;

			run_commanded_process(&tracees, &tracee_list, new_cmd_pid, n_insns, 0);
		}
		close(sock_fd);

	#else


		
		cpu_assigned = addToExp(rel_cpu_speed, n_round_insns);

		if(cpu_assigned <= 0){

			if((255 - errno) > 0 && (255 - errno) < n_cpus){
				cpu_assigned = 255 - errno;
				LOG("Tracer: %d, Assigned CPU = %d\n", tracer_id, cpu_assigned);
				errno = 0;	
			}
			else{
				LOG("Registration Error. errno = %d\n", errno);
				exit(-1);
			}
		}
		else{
			LOG("Tracer: %d, Assigned cpu = %d\n", tracer_id, cpu_assigned);
		}

		//fp = fopen("/proc/dilation/status","a");
		fp = open("/proc/status", O_RDWR);

		if(fp == -1){
			LOG("PROC file open error\n");
			exit(-1);;
		}


		while(1){
			flush_buffer(nxt_cmd, MAX_PAYLOAD);
			tail_ptr = 0;
			new_cmd_pid = 0;
			n_insns = 0;
			read_ret = -1;
			while(read_ret == -1){
				read_ret = read(fp, nxt_cmd,fp);
				//usleep(10000);
			}
			LOG("Tracer: %d, Received Command: %s\n", tracer_id, nxt_cmd);
			while(tail_ptr != -1){
				tail_ptr = get_next_command_tuple(nxt_cmd, tail_ptr, &new_cmd_pid, &n_insns);

				if(tail_ptr == -1 && new_cmd_pid == -1){
					LOG("Tracer: %d, STOP command received. Stopping tracer\n", tracer_id);
					goto end;
				}

				if(new_cmd_pid == 0)
					break;

				#ifdef DEBUG_VERBOSE
				LOG("Tracer: %d, Running Child: %d for %d instructions\n", tracer_id, new_cmd_pid, n_insns);
				#endif

				run_commanded_process(&tracees, &tracee_list, new_cmd_pid, n_insns, cpu_assigned);
				//usleep(10000);
			}

			flush_buffer(command,MAX_BUF_SIZ);
			sprintf(command, "%c,%s,", TRACER_RESULTS,"0");

			#ifdef DEBUG_VERBOSE
			LOG("Tracer: %d: Writing Cmd Results\n", tracer_id);
			#endif
			
			write(fp,command, strlen(command));
		}
		end:
		close(fp);

	#endif

	
	llist_destroy(&tracee_list);
	hmap_destroy(&tracees);	


	
	return 0;

}
