#include "dilation_module.h"

/***
Contains some basic functions (such as atoi), as well as some TimeKeeper debug functions.
***/

int get_next_value (char *write_buffer);
int atoi(char *s);
struct task_struct* find_task_by_pid(unsigned int nr);
int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task);

void print_proc_info(char *write_buffer);

void print_children_info_proc(char *write_buffer);
void print_children_info(struct task_struct *aTask);
void print_children_info_pid(int pid);

int find_children_info(struct task_struct *aTask, int pid);
void print_threads_proc(char *write_buffer);

struct sock *nl_sk = NULL;
struct task_struct *loop_task;

void send_a_message_proc(char * write_buffer);
void send_a_message(int pid);

extern s64 Sim_time_scale;
extern struct list_head exp_list;
//struct poll_list;
extern struct poll_list {
    struct poll_list *next;
    int len;
    struct pollfd entries[0];
};
extern struct poll_helper_struct;
extern hashmap poll_process_lookup;

												
	




/***
Wrapper for sending a message to userspace
***/
void send_a_message_proc(char * write_buffer) {
        int pid;
        pid = atoi(write_buffer);
        printk(KERN_INFO "TimeKeeper : Send a message proc: called from send_a_msg_proc\n");
        send_a_message(pid);
}

/***
Send a message from the Kernel to Userspace (to let the process know all LXCs have advanced to a
certain point.
***/
void send_a_message(int pid) {
    struct nlmsghdr *nlh;
    struct sk_buff *skb_out;
    int msg_size;
    char *msg = "";
    int res;

    msg_size = strlen(msg);

    skb_out = nlmsg_new(msg_size, 0);

    if (!skb_out)
    {

        printk(KERN_ERR "Send a message: Failed to allocate new skb\n");
        return;

    }
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), msg, msg_size);

    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "TimeKeeper: Send a message: Error while sending bak to user %d, pid = %d\n", res, pid);
    }
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
        return (i + 1);
}
/***
 Convert string to integer
***/
int atoi(char *s)
{
        int i,n;
        n = 0;
        for(i = 0; *(s+i) >= '0' && *(s+i) <= '9'; i++)
                n = 10*n + *(s+i) - '0';
        return n;
}


/***
Given a pid, returns a pointer to the associated task_struct
***/
struct task_struct* find_task_by_pid(unsigned int nr)
{
        struct task_struct* task;
        rcu_read_lock();
        task=pid_task(find_vpid(nr), PIDTYPE_PID);
        rcu_read_unlock();
        return task;
}


/***
My implementation of the kill system call. Will send a signal to a container. Used for freezing/unfreezing containers
***/
int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task) {
        struct siginfo info;
        int returnVal;
        info.si_signo = sig;
        info.si_errno = 0;
        info.si_code = SI_USER;
        if ((returnVal = send_sig_info(sig, &info, killTask)) != 0)
        {
                if (dilation_task != NULL)
                {
                        dilation_task->stopped = -1;
						printk(KERN_INFO "TimeKeeper: Kill: Error sending kill msg for pid %d\n", dilation_task->linux_task->pid);
                }
        }
        return returnVal;
}

/***
Wrapper for printing all children of a process - for debugging
***/
void print_children_info_proc(char *write_buffer) {
	int pid;
	struct task_struct* aTask;

	pid = atoi(write_buffer);
	aTask = find_task_by_pid(pid);
	if (aTask != NULL) {
		printk(KERN_INFO "TimeKeeper: Print Children Info Proc: Finding children pids for %d\n", aTask->pid);
		print_children_info(aTask);
	}
}

/*
Print all children of a process - for debugging
*/
void print_children_info_pid(int pid) {
        struct task_struct* aTask;

        aTask = find_task_by_pid(pid);
        if (aTask != NULL) {
                printk(KERN_INFO "TimeKeeper: Print Children Info PID: Finding children pids for %d\n", aTask->pid);
                print_children_info(aTask);
        }
}

/*
The recursive function to print all children of a process - for debugging
*/
void print_children_info(struct task_struct *aTask) {
        struct list_head *list;
        struct task_struct *taskRecurse;
        if (aTask == NULL) {
                printk(KERN_INFO "TimeKeeper: Print Children Info: Task does not exist\n");
                return;
        }
        if (aTask->pid == 0) {
                return;
        }

        list_for_each(list, &aTask->children)
        {
                taskRecurse = list_entry(list, struct task_struct, sibling);
                if (taskRecurse == NULL || taskRecurse->pid == 0) {
                        return;
                }
                print_children_info(taskRecurse);
        }
}

/***
Given a starting task and pid, searches all children of that task, looking for a matching PID. If we return 1,
then there exists a child in this container with that pid, else -1
***/
int find_children_info(struct task_struct *aTask, int pid) {
    struct list_head *list;
    struct task_struct *taskRecurse;
	struct task_struct *me;
	struct task_struct *t;

    if (aTask == NULL) {
            printk(KERN_INFO "TimeKeeper: Find Children Info: Task does not exist\n");
            return -1;
    }
    if (pid == aTask->pid) {
            printk(KERN_INFO "TimeKeeper: Find Children Info: Task exists for this pid : %d in the experiment \n", pid);
            return 1;
    }

    if (aTask->pid == 0) {
            return -1;
    }


	me = aTask;
	t = me;
	do {
		if (t->pid == pid) {
			return 1;
		}

	} while_each_thread(me, t);


    list_for_each(list, &aTask->children)
    {
            taskRecurse = list_entry(list, struct task_struct, sibling);
            if (taskRecurse == NULL) {
                    return -1;
            }
			if (taskRecurse->pid == 0) {
				return -1;
			}
			
           	if (find_children_info(taskRecurse, pid) == 1) {
				return 1;
			}
    }
	return -1;
}


/***
Compares 2 task_structs, outputs things such as the TDF, virt_start_time and so forth
***/
void print_proc_info(char *write_buffer) {
        int pid,pid2, value;
        struct task_struct* aTask;
        struct task_struct* aTask2;
        s64 now;
        s32 rem;
        struct timeval now_timeval;
        s64 tempTime;
        struct timeval ktv, ktv2;
        s64 real_running_time;
        s64 dilated_running_time;

        pid = atoi(write_buffer);
        aTask = find_task_by_pid(pid);
        value = get_next_value(write_buffer);
        pid2 = atoi(write_buffer + value);
        aTask2 = find_task_by_pid(pid2);

        if (aTask != NULL && aTask2 != NULL) {
        	do_gettimeofday(&now_timeval);
        	now = timeval_to_ns(&now_timeval);
        	printk(KERN_INFO "TimeKeeper: ---------------STARTING COMPARE--------------\n");
                real_running_time = now - aTask->virt_start_time;
                if (aTask->dilation_factor > 0) {
						dilated_running_time = div_s64_rem( (real_running_time - aTask->past_physical_time)*1000 ,aTask->dilation_factor,&rem) + aTask->past_virtual_time;
                        tempTime = dilated_running_time + aTask->virt_start_time;
                }
                else {
                        dilated_running_time = (real_running_time - aTask->past_physical_time) + aTask->past_virtual_time;
                        tempTime = dilated_running_time + aTask->virt_start_time;
                }
                printk(KERN_INFO "TimeKeeper: now: %lld d_r_t: %lld time: %lld\n", now, dilated_running_time, tempTime);
                ktv = ns_to_timeval(tempTime);
                real_running_time = now - aTask2->virt_start_time;
                if (aTask2->dilation_factor > 0) {
			dilated_running_time = div_s64_rem( (real_running_time - aTask2->past_physical_time)*1000 ,aTask2->dilation_factor,&rem) + aTask2->past_virtual_time;
                        tempTime = dilated_running_time + aTask2->virt_start_time;
                }
                else {
                        dilated_running_time = (real_running_time - aTask2->past_physical_time) + aTask2->past_virtual_time;
                        tempTime = dilated_running_time + aTask2->virt_start_time;
                }
                ktv2 = ns_to_timeval(tempTime);
        	printk(KERN_INFO "TimeKeeper: PID: %d %d TGID: %d %d\n", aTask->pid, aTask2->pid, aTask->tgid, aTask2->tgid);
	        printk(KERN_INFO "TimeKeeper: Dilation %d %d\n", aTask->dilation_factor, aTask2->dilation_factor);
        	printk(KERN_INFO "TimeKeeper: virt_start_time %lld %lld\n", aTask->virt_start_time, aTask2->virt_start_time);
	        printk(KERN_INFO "TimeKeeper: diff %lld %lld\n", now - aTask->virt_start_time, now - aTask2->virt_start_time);
        	printk(KERN_INFO "TimeKeeper: current seconds %ld %ld\n", ktv.tv_sec, ktv2.tv_sec);
	        printk(KERN_INFO "TimeKeeper: freeze_time %lld %lld\n", aTask->freeze_time, aTask2->freeze_time);
        	printk(KERN_INFO "TimeKeeper: past_physical_time %lld %lld\n", aTask->past_physical_time, aTask2->past_physical_time);
	        printk(KERN_INFO "TimeKeeper: past_virtual_time %lld %lld\n", aTask->past_virtual_time, aTask2->past_virtual_time);
        }

        else {
                printk(KERN_INFO "TimeKeeper: A task is null\n");
        }
        return;
}

/***
Prints all threads of a process - for debugging
***/
void print_threads_proc(char *write_buffer) {
	struct task_struct *me;
	struct task_struct *t;
	int pid;
	pid = atoi(write_buffer);
	me = find_task_by_pid(pid);
	t = me;
	printk(KERN_INFO "TimeKeeper: Print Threads Proc: Finding threads for pid: %d\n", me->pid);
	do {
    		printk(KERN_INFO "TimeKeeper: Print Threads Proc: Pid: %d %d\n",t->pid, t->tgid);
	} while_each_thread(me, t);
	return;
	}


/***
Increment the past physical times of the process and all it's children to the specified argument 
***/
/*
void increment_all_past_physical_times_recurse(struct task_struct * aTask, s64 increment){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	unsigned long flags;


	if (aTask == NULL) {
		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Set all past physical times: Task does not exist\n");
		return;
	}

	if (aTask->pid == 0) {
		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: Set all past physical times: pid 0 error\n");
		return;
	}
	
	if(increment < 0)
	    return;

    acquire_irq_lock(&aTask->dialation_lock,flags);
	aTask->past_physical_time = aTask->past_physical_time + increment;
	release_irq_lock(&aTask->dialation_lock,flags);
	
	me = aTask;
	t = me;
	do {
	    if(t->pid != aTask->pid){
    		acquire_irq_lock(&t->dialation_lock,flags);
	    	t->past_physical_time = t->past_physical_time + increment;
	    	release_irq_lock(&t->dialation_lock,flags);
		}
		
	} while_each_thread(me, t);

    list_for_each(list, &aTask->children)
    {

        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
                continue;
        }

        increment_all_past_physical_times_recurse(taskRecurse, increment);
    }
}
*/


int find_in_schedule_list(struct dilation_task_struct * lxc, int pid) {

	int i = 0;
	lxc_schedule_elem * curr;
	if(lxc != NULL) {
		for(i = 0; i < schedule_list_size(lxc); i++){
			curr = llist_get(&lxc->schedule_queue, i);
			if(curr != NULL) {
				if(curr->pid == pid)
					return 1;
			}
		}
	
	}
	
	return 0;
}

/*** 
Add to tail of schedule queue 
***/
int add_to_schedule_list(struct dilation_task_struct * lxc, struct task_struct *new_task, s64 FREEZE_QUANTUM, s64 highest_dilation){



	int dilation_factor;
	int dil;
    s64 temp_proc;
    s64 temp_high;
    s32 rem;
	struct task_struct *me;
    struct task_struct *t;
	s64 window_duration;
	s64 expected_increase;
	int n_threads = 0;
	s64 base_time_quanta;
	unsigned long flags;



	if(new_task == NULL || lxc == NULL)
		return -1;

	/* child already exists. don't add */
	if(hmap_get_abs(&lxc->valid_children,new_task->pid) != NULL) 
	{	
		if(find_in_schedule_list(lxc,new_task->pid) == 0)
			printk(KERN_INFO "Add to Schedule List Error: Found in map but not in list. Pid = %d\n", new_task->pid);
		return 0;
	}


	lxc_schedule_elem * new_element = (lxc_schedule_elem *)kmalloc(sizeof(lxc_schedule_elem), GFP_KERNEL);
	if(new_element == NULL)
		return -1;


	new_element->static_priority = new_task->static_prio;
	

    /* temp_proc and temp_high are temporary dilations for the container and leader respectively.
    this is done just to make sure the Math works (no divide by 0 errors if the TDF is 0, by making the temp TDF 1) */
    temp_proc = 0;
    temp_high = 0;

    if (highest_dilation == 0)
            temp_high = 1;
    else if (highest_dilation < 0)
            temp_high = highest_dilation*-1;


	acquire_irq_lock(&new_task->dialation_lock,flags);

	new_task->dilation_factor = lxc->linux_task->dilation_factor;
	
	/*if(lxc->last_run != NULL && find_task_by_pid(lxc->last_run->pid) != NULL) {
	   	new_task->freeze_time = lxc->last_run->curr_task->freeze_time;
    	new_task->past_physical_time = lxc->last_run->curr_task->past_physical_time;
    	new_task->past_virtual_time = lxc->last_run->curr_task->past_virtual_time;
    	new_task->virt_start_time = lxc->last_run->curr_task->virt_start_time;
    }
    else{*/
        new_task->virt_start_time = lxc->linux_task->virt_start_time;
    	new_task->freeze_time = lxc->linux_task->freeze_time;
    	new_task->past_physical_time = lxc->linux_task->past_physical_time;
    	new_task->past_virtual_time = lxc->linux_task->past_virtual_time;
    //}
	
	

    dil = new_task->dilation_factor;

    if (dil == 0)
       temp_proc = 1;
    else if (dil < 0)
       temp_proc = dil*-1;


	me = new_task;
	t = me;
	n_threads = 0;

	do {
		bitmap_zero((&t->cpus_allowed)->bits, 8);
   		cpumask_set_cpu(lxc->cpu_assignment,&t->cpus_allowed);
		n_threads++;
	} while_each_thread(me, t);



	if(dil == 0){
		base_time_quanta = 1*Sim_time_scale;
	}
	else{
		base_time_quanta = div_s64_rem(new_task->dilation_factor,1000,&rem)*Sim_time_scale;
	}

	
	if(new_element->static_priority  <= 120){

		/* In ms- Normal Process allotted 200 us vt quanta. Quanta of higher priority processes scaled according to linux spec*/

        if(new_task->pid == lxc->linux_task->pid)
            base_time_quanta = base_time_quanta*100000;
        else {
    		//base_time_quanta = base_time_quanta*200000;
    		base_time_quanta = base_time_quanta*(140 - new_element->static_priority)* 10000; 
    		
    	}
    		
		lxc->rr_run_time += 1;
	}
	else{

		/* 200 us for now for all lower priority process. TODO */		
		if(new_task->pid == lxc->linux_task->pid)
            base_time_quanta = base_time_quanta*100000;
        else
    		base_time_quanta = base_time_quanta*200000; 
		lxc->rr_run_time += 1;
	}
	
	
	printk(KERN_INFO "TimeKeeper: Add To Schedule List: PID : %d, LXC: %d, Base Quanta : %lld. N_threads : %d. Expected_increase : %lld\n", new_task->pid, lxc->linux_task->pid, base_time_quanta, n_threads, expected_increase);
	release_irq_lock(&new_task->dialation_lock,flags);

	new_element->share_factor = base_time_quanta;
	new_element->curr_task = new_task;
	new_element->pid = new_task->pid;
	new_element->duration_left = base_time_quanta;

	/* append to tail of schedule queue */
	llist_append(&lxc->schedule_queue, new_element); 


	bitmap_zero((&new_task->cpus_allowed)->bits, 8);
    cpumask_set_cpu(lxc->cpu_assignment,&new_task->cpus_allowed);

	struct sched_param sp;
	sp.sched_priority = 99;
	sched_setscheduler(new_task, SCHED_RR, &sp);
	hmap_put_abs(&lxc->valid_children,new_element->pid, new_element);
	

	return 0;


}

/*** 
Remove head of schedule queue and return the task_struct of the head element 
***/
struct task_struct * pop_schedule_list(struct dilation_task_struct * lxc){

	if(lxc == NULL)
		return NULL;

	lxc_schedule_elem * head;
	head = llist_pop(&lxc->schedule_queue);
	struct task_struct * curr_task;

	if(head != NULL){
		curr_task = head->curr_task;
		hmap_remove_abs(&lxc->valid_children, head->pid);
		kfree(head);
		return curr_task;
	}


	return NULL;

}



/***
Get pointer to task_Struct of head but don't remove the element from the schedule list 
***/
lxc_schedule_elem * schedule_list_get_head(struct dilation_task_struct * lxc){

	if(lxc == NULL) {
		printk(KERN_INFO "Get Head: LXC is Null\n");
		return NULL;
	}


	return llist_get(&lxc->schedule_queue, 0);


}




/***
Requeue schedule queue, i.e pop from head and add to tail 
***/
void requeue_schedule_list(struct dilation_task_struct * lxc){

	if(lxc == NULL)
		return;
	llist_requeue(&lxc->schedule_queue);

}

void clean_up_schedule_list(struct dilation_task_struct * lxc){

	struct task_struct * curr_task = pop_schedule_list(lxc);
	while(curr_task != NULL){
		curr_task = pop_schedule_list(lxc);
	}

	hmap_destroy(&lxc->valid_children);
	llist_destroy(&lxc->schedule_queue);	

}

int schedule_list_size(struct dilation_task_struct * lxc){

	if(lxc == NULL)
		return 0;
	
	return llist_size(&lxc->schedule_queue);

}



/*** Wrappers for performing dialated poll and select system calls ***/

static inline unsigned int do_dialated_pollfd(struct pollfd *pollfd, poll_table *pwait, bool *can_busy_poll, unsigned int busy_flag,struct task_struct * tsk)
{
	unsigned int mask;
	int fd;

	mask = 0;
	fd = pollfd->fd;
	if (fd >= 0) {
		struct fd f = fdget(fd);
		mask = POLLNVAL;
		if (f.file) {
			mask = DEFAULT_POLLMASK;
			if (f.file->f_op->poll) {
				pwait->_key = pollfd->events|POLLERR|POLLHUP;
				pwait->_key |= busy_flag;
				mask = f.file->f_op->poll(f.file, pwait);
				if (mask & busy_flag)
					*can_busy_poll = true;
			}
			/* Mask out unneeded events. */
			mask &= pollfd->events | POLLERR | POLLHUP;
			fdput(f);
		}
	}
	pollfd->revents = mask;

	return mask;
}

int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait,struct task_struct * tsk)
{
	poll_table* pt = &wait->pt;
	int count = 0;
	unsigned int busy_flag = 0;
	unsigned long busy_end = 0;
	

	struct poll_list *walk;
	bool can_busy_loop = false;

	for (walk = list; walk != NULL; walk = walk->next) {
		struct pollfd * pfd, * pfd_end;
		pfd = walk->entries;
		pfd_end = pfd + walk->len;
		for (; pfd != pfd_end; pfd++) {
			/*
			 * Fish for events. If we found one, record it
			 * and kill poll_table->_qproc, so we don't
			 * needlessly register any other waiters after
			 * this. They'll get immediately deregistered
			 * when we break out and return.
			 */
			if (do_dialated_pollfd(pfd, pt, &can_busy_loop,
				      busy_flag,tsk)) {
				count++;
				pt->_qproc = NULL;
				/* found something, stop busy polling */
				busy_flag = 0;
				can_busy_loop = false;
			}
		}
	}
	/*
	 * All waiters have already been registered, so don't provide
	 * a poll_table->_qproc to them on the next loop iteration.
	 */
	pt->_qproc = NULL;
		
	
	return count;
}


int max_sel_fd(unsigned long n, fd_set_bits *fds,struct task_struct * tsk)
{
	unsigned long *open_fds;
	unsigned long set;
	int max;
	struct fdtable *fdt;

	/* handle last in-complete long-word first */
	set = ~(~0UL << (n & (BITS_PER_LONG-1)));
	n /= BITS_PER_LONG;
	fdt = files_fdtable(current->files);
	open_fds = fdt->open_fds + n;
	printk(KERN_INFO "TimeKeeper: max_sel_fd : Pid = %d, n = %lu\n",current->pid, n);

	max = 0;
	if (set) {
		set &= BITS(fds, n);
		if (set) {
			if (!(set & ~*open_fds))
				goto get_max;
			return -EBADF;
		}
	}
	while (n) {
		open_fds--;
		n--;
		set = BITS(fds, n);
		if (!set)
			continue;
		if (set & ~*open_fds)
			return -EBADF;
		if (max)
			continue;
get_max:
		do {
			max++;
			set >>= 1;
		} while (set);
		max += n * BITS_PER_LONG;
	}

	return max;
}

void wait_k_set(poll_table *wait, unsigned long in,unsigned long out, unsigned long bit, unsigned int ll_flag)
{
         wait->_key = POLLEX_SET | ll_flag;
         if (in & bit)
                 wait->_key |= POLLIN_SET;
         if (out & bit)
                 wait->_key |= POLLOUT_SET;
}


int do_dialated_select(int n, fd_set_bits *fds,struct task_struct * tsk)
{
	ktime_t expire, *to = NULL;
	struct poll_wqueues table;
	poll_table *wait;
	int retval, i, timed_out = 0;
	unsigned long slack = 0;
	unsigned int busy_flag = 0;
	unsigned long busy_end = 0;


	printk(KERN_INFO "TimeKeeper: Do dialated Select: Entered. Pid = %d\n", current->pid);

	rcu_read_lock();
	retval = max_sel_fd(n, fds,tsk);
	rcu_read_unlock();

	
	printk(KERN_INFO "TimeKeeper: Do dialated Select: Returned from Max_Sel_fd. Pid = %d\n", current->pid);

	if (retval < 0)
		return retval;
	n = retval;

	poll_initwait(&table);
	wait = &table.pt;
	retval = 0;

	unsigned long *rinp, *routp, *rexp, *inp, *outp, *exp;
	bool can_busy_loop = false;
	inp = fds->in; outp = fds->out; exp = fds->ex;
	rinp = fds->res_in; routp = fds->res_out; rexp = fds->res_ex;
	for (i = 0; i < n; ++rinp, ++routp, ++rexp) {
		unsigned long in, out, ex, all_bits, bit = 1, mask, j;
		unsigned long res_in = 0, res_out = 0, res_ex = 0;

		in = *inp++; out = *outp++; ex = *exp++;
		all_bits = in | out | ex;
		if (all_bits == 0) {
			i += BITS_PER_LONG;
			continue;
		}

		for (j = 0; j < BITS_PER_LONG; ++j, ++i, bit <<= 1) {
			struct fd f;
			if (i >= n)
				break;
			if (!(bit & all_bits))
				continue;
			f = fdget(i);
			if (f.file) {
				const struct file_operations *f_op;
				f_op = f.file->f_op;
				mask = DEFAULT_POLLMASK;
				if (f_op->poll) {
					wait_k_set(wait, in, out,
						     bit, busy_flag);
					mask = (*f_op->poll)(f.file, wait);
				}
				//fdput(f);
				if ((mask & POLLIN_SET) && (in & bit)) {
					res_in |= bit;
					retval++;
					wait->_qproc = NULL;
				}
				if ((mask & POLLOUT_SET) && (out & bit)) {
					res_out |= bit;
					retval++;
					wait->_qproc = NULL;
				}
				if ((mask & POLLEX_SET) && (ex & bit)) {
					res_ex |= bit;
					retval++;
					wait->_qproc = NULL;
				}
				/* got something, stop busy polling */
				if (retval) {
					can_busy_loop = false;
					busy_flag = 0;
					/*
				 * only remember a returned
				 * POLL_BUSY_LOOP if we asked for it
				 */
				} else if (busy_flag & mask)
					can_busy_loop = true;
			}
		}
		if (res_in)
			*rinp = res_in;
		if (res_out)
			*routp = res_out;
		if (res_ex)
			*rexp = res_ex;
	}
	wait->_qproc = NULL;

	if (table.error) {
		retval = table.error;
	}


	poll_freewait(&table);
	printk(KERN_INFO "TimeKeeper: Do dialated Select: Returned function. Pid = %d\n", current->pid);

	return retval;
}
