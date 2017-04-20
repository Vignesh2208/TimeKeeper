#include "dilation_module.h"


/*
Contains the code for acquiring the syscall table, as well as the 4 system calls Timekeeper currently hooks.
*/
unsigned long **aquire_sys_call_table(void);


asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp);
asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp, struct timespec __user *rmtp);
asmlinkage int sys_poll_new(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int (*ref_sys_poll_dialated)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int sys_select_new(int k, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage int (*ref_sys_select)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage int (*ref_sys_select_dialated)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);


extern struct list_head exp_list;
//struct poll_list;
extern struct poll_list {
    struct poll_list *next;
    int len;
    struct pollfd entries[0];
};
extern struct poll_helper_struct;
extern struct select_helper_struct;
extern struct sleep_helper_struct;
extern hashmap poll_process_lookup;
extern hashmap select_process_lookup;
extern hashmap sleep_process_lookup;

extern int find_children_info(struct task_struct* aTask, int pid);
extern int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task);
extern int experiment_stopped;
extern int experiment_type;
extern s64 Sim_time_scale;
extern s64 expected_increase;
extern atomic_t n_active_syscalls;
extern atomic_t experiment_stopping;


extern int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait,struct task_struct * tsk);
extern int do_dialated_select(int n, fd_set_bits *fds,struct task_struct * tsk);

s64 get_dilated_time(struct task_struct * task)
{
	s64 temp_past_physical_time;
	struct timeval tv;
	do_gettimeofday(&tv);
	s64 now = timeval_to_ns(&tv);

	if(task->virt_start_time != 0){

		/* use virtual time of the leader thread */
		if (task->group_leader != task) { 
           	task = task->group_leader;
        }
	
		s32 rem;
		s64 real_running_time;
		s64 dilated_running_time;
		
		real_running_time = now - task->virt_start_time;
		
		if (task->freeze_time != 0) {
			temp_past_physical_time = task->past_physical_time + (now - task->freeze_time);		
		}
		else{
		    temp_past_physical_time = task->past_physical_time;
		    
		}

		if (task->dilation_factor > 0) {
			dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*1000 ,task->dilation_factor,&rem) + task->past_virtual_time;
			now = dilated_running_time + task->virt_start_time;
		}
		else if (task->dilation_factor < 0) {
			dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*(task->dilation_factor*-1),1000,&rem) + task->past_virtual_time;
			now =  dilated_running_time + task->virt_start_time;
		}
		else {
			dilated_running_time = (real_running_time - temp_past_physical_time) + task->past_virtual_time;
			now = dilated_running_time + task->virt_start_time;
		}
	
	}

	return now;

}

/***
Hooks the sleep system call, so the process will wake up when it reaches the experiment virtual time,
not the system time
***/
asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp) {
	struct list_head *pos;
	struct list_head *n;
	struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;
	s64 now_new;
	s32 rem;
	s64 real_running_time;
	s64 dilated_running_time;
	current_task = current;
	unsigned long flags;
	int ret;
	int is_dialated = 0;
	struct sleep_helper_struct helper;	
	struct sleep_helper_struct * sleep_helper = &helper;
	struct list_head *list;
    struct task_struct *taskRecurse;

	

	acquire_irq_lock(&current->dialation_lock,flags);
	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET && atomic_read(&experiment_stopping) == 0)
	{		
		atomic_inc(&n_active_syscalls);
		is_dialated = 1;
    	do_gettimeofday(&ktv);
		now = timeval_to_ns(&ktv);			
		now_new = get_dilated_time(current);

		init_waitqueue_head(&sleep_helper->w_queue);
		atomic_set(&sleep_helper->done,0);
		hmap_put_abs(&sleep_process_lookup,current->pid,sleep_helper);
		release_irq_lock(&current->dialation_lock,flags);
		
		s64 wakeup_time = now_new + ((rqtp->tv_sec*1000000000) + rqtp->tv_nsec)*Sim_time_scale;
		PDEBUG_V("Sys Sleep: PID : %d, Sleep Secs: %d Nano Secs: %llu, New wake up time : %lld\n",current->pid, rqtp->tv_sec, rqtp->tv_nsec, wakeup_time); 
		
		while(now_new < wakeup_time) {
			set_current_state(TASK_INTERRUPTIBLE);
			wait_event(sleep_helper->w_queue,atomic_read(&sleep_helper->done) != 0);
			set_current_state(TASK_RUNNING);
			atomic_set(&sleep_helper->done,0);
			
			now_new = get_dilated_time(current);
			if(now_new < wakeup_time){  			
			    if(current->freeze_time == 0 && atomic_read(&experiment_stopping) == 0){
					kill(current,SIGCONT,NULL);
					list_for_each(list, &current->children)
 		   			{
            			taskRecurse = list_entry(list, struct task_struct, sibling);     	
						if (taskRecurse!= NULL &&taskRecurse->pid == 0) {
							    continue;
						}
						//taskRecurse->freeze_time = 0;
						/* Let other children use the cpu */
						if(taskRecurse != NULL)
						kill(taskRecurse, SIGCONT, NULL); 
					}   
				}
			}

		    
		    if(atomic_read(&experiment_stopping) == 1 || experiment_stopped != RUNNING)
		    	break;
		    
			
        }
		acquire_irq_lock(&current->dialation_lock,flags);
		hmap_remove_abs(&sleep_process_lookup,current->pid);
		release_irq_lock(&current->dialation_lock,flags);		

		s64 diff = 0;
		diff = now_new - wakeup_time;
		PDEBUG_V("Sys Sleep: Resumed Sleep Process Expiry %d. Resume time = %llu. Difference = %llu\n",current->pid, now_new,diff );
		
		atomic_dec(&n_active_syscalls);
		return 0; 
		
		revert_sleep:
		release_irq_lock(&current->dialation_lock,flags);
		atomic_dec(&n_active_syscalls);
		
		return 0;
	} 
	
	
	release_irq_lock(&current->dialation_lock,flags);
    return ref_sys_sleep(rqtp,rmtp);
}



asmlinkage int sys_select_new(int k, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp){


	struct list_head *pos;
	struct list_head *n;
	struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;
	s64 now_new;
	s32 rem;
	s64 real_running_time;
	s64 dilated_running_time;
	current_task = current;
	struct timespec end_time, *to = NULL;
	int ret;
	int err = -EFAULT, fdcount, len, size;
	s64 secs_to_sleep;
	s64 nsecs_to_sleep;
	struct timeval tv;
	struct timeval rtv;
	int max_fds;
	struct fdtable *fdt;
	long stack_fds[SELECT_STACK_ALLOC/sizeof(long)];
	void * bits;
	unsigned long flags;
	s64 time_to_sleep = 0;
	int is_dialated = 0;
	struct select_helper_struct  helper;
	struct select_helper_struct * select_helper = &helper;
	struct list_head *list;
    struct task_struct *taskRecurse;	
	
	rcu_read_lock();
	fdt = files_fdtable(current->files);
	max_fds = fdt->max_fds;
	rcu_read_unlock();
	if (k > max_fds)
		k = max_fds;


	
	acquire_irq_lock(&current->dialation_lock,flags);
	if(experiment_stopped == RUNNING && current->virt_start_time != NOTSET && tvp != NULL && atomic_read(&experiment_stopping) == 0){	

		atomic_inc(&n_active_syscalls);	
		is_dialated = 1;
		if (copy_from_user(&tv, tvp, sizeof(tv)))
			goto revert_select;

		secs_to_sleep = tv.tv_sec + (tv.tv_usec / USEC_PER_SEC);
		nsecs_to_sleep = (tv.tv_usec % USEC_PER_SEC) * NSEC_PER_USEC;
		time_to_sleep = (secs_to_sleep*1000000000) + nsecs_to_sleep;
		if(experiment_type != CS && time_to_sleep < expected_increase)
			goto revert_select;
		    

		ret = -EINVAL;
		if (k < 0)
			goto revert_select;

		select_helper->bits = stack_fds;
		init_waitqueue_head(&select_helper->w_queue);
		atomic_set(&select_helper->done,0);
		select_helper->ret = -EFAULT;


		select_helper->n = k;
		size = FDS_BYTES(k);
		if (size > sizeof(stack_fds) / 6) {
			
			ret = -ENOMEM;
			select_helper->bits = kmalloc(6 * size, GFP_KERNEL);
			if (!select_helper->bits) {
				goto revert_select;
			}
		}
		bits = select_helper->bits;
		select_helper->fds.in      = bits;
		select_helper->fds.out     = bits +   size;
		select_helper->fds.ex      = bits + 2*size;
		select_helper->fds.res_in  = bits + 3*size;
		select_helper->fds.res_out = bits + 4*size;
		select_helper->fds.res_ex  = bits + 5*size;

		if ((ret = get_fd_set(k, inp, select_helper->fds.in)) ||
		    (ret = get_fd_set(k, outp, select_helper->fds.out)) ||
		    (ret = get_fd_set(k, exp, select_helper->fds.ex))) {
		    
		    	if(select_helper->bits != stack_fds)
					kfree(select_helper->bits);
				goto revert_select;
		}

		zero_fd_set(k, select_helper->fds.res_in);
		zero_fd_set(k, select_helper->fds.res_out);
		zero_fd_set(k, select_helper->fds.res_ex);
		
		memset(&rtv, 0, sizeof(rtv));
		copy_to_user(tvp, &rtv, sizeof(rtv));
		hmap_put_abs(&select_process_lookup, current->pid, select_helper);
		release_irq_lock(&current->dialation_lock,flags);
		
		do_gettimeofday(&ktv);
		now = timeval_to_ns(&ktv);	
		now_new = get_dilated_time(current);
		s64 wakeup_time;
		
		
		wakeup_time = now_new + ((secs_to_sleep*1000000000) + nsecs_to_sleep)*Sim_time_scale; 	
		PDEBUG_V("Sys Select: Select Process Waiting %d. Timeout sec %d, nsec %d, wakeup_time = %llu\n",current->pid,secs_to_sleep,nsecs_to_sleep,wakeup_time);
		
		while(1){
			
			set_current_state(TASK_INTERRUPTIBLE);
			if(now_new < wakeup_time){
			
			
				if(atomic_read(&select_helper->done) != 0) {							
					atomic_set(&select_helper->done,0);	
					ret = do_dialated_select(select_helper->n,&select_helper->fds,current);
					if(ret || select_helper->ret == FINISHED || atomic_read(&experiment_stopping) == 1){
						select_helper->ret = ret;
						break;
					}
				}
				wait_event(select_helper->w_queue,atomic_read(&select_helper->done) != 0);
				set_current_state(TASK_RUNNING);		


			}
			
			
			now_new = get_dilated_time(current);
			if(now_new < wakeup_time) {
				acquire_irq_lock(&current->dialation_lock,flags);	
			    if(current->freeze_time == 0 && atomic_read(&experiment_stopping) == 0) {
			        kill(current,SIGCONT,NULL);
			        list_for_each(list, &current->children)
 		   			{
            			taskRecurse = list_entry(list, struct task_struct, sibling);     	
						if (taskRecurse != NULL && taskRecurse->pid == 0) {
							    continue;
						}
						//taskRecurse->freeze_time = 0;
						/* Let other children use the cpu */
						if(taskRecurse != NULL)
						kill(taskRecurse, SIGCONT, NULL); 
					}
			        
				}
			    release_irq_lock(&current->dialation_lock,flags);
			}
			else{
				select_helper->ret = 0;
			    break;
			}
		}		
		acquire_irq_lock(&current->dialation_lock,flags);	
		hmap_remove_abs(&select_process_lookup, current->pid);
		release_irq_lock(&current->dialation_lock,flags);
		
		s64 diff = 0;
		if(wakeup_time >  now_new){
			diff = wakeup_time - now_new; 
			PDEBUG_V("Sys Select: Resumed Select Process Early %d. Resume time = %llu. Difference = %llu\n",current->pid, now_new,diff );

		}
		else{
			diff = now_new - wakeup_time;
			PDEBUG_V("Sys Select: Resumed Select Process Expiry %d. Resume time = %llu. Difference = %llu\n",current->pid, now_new,diff );
		}		 
		
		ret = select_helper->ret;
		if(ret < 0)
			goto out;
		


		if (set_fd_set(k, inp, select_helper->fds.res_in) ||
		    set_fd_set(k, outp, select_helper->fds.res_out) ||
		    set_fd_set(k, exp, select_helper->fds.res_ex))
			ret = -EFAULT;

		

		out:
		
		if(bits != stack_fds)
			kfree(select_helper->bits);

		out_nofds:
		PDEBUG_V("Sys Select: Select finished PID %d\n",current->pid);
		atomic_dec(&n_active_syscalls);
		return ret;
		
		revert_select:
		release_irq_lock(&current->dialation_lock,flags);	
		atomic_dec(&n_active_syscalls);
		
		return ref_sys_select(k,inp,outp,exp,tvp);
	}
	
	release_irq_lock(&current->dialation_lock,flags);	
	return ref_sys_select(k,inp,outp,exp,tvp);
}

asmlinkage int sys_poll_new(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs){

	struct list_head *pos;
	struct list_head *n;
	struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;
	s64 now_new;
	s32 rem;
	s64 real_running_time;
	s64 dilated_running_time;
	current_task = current;
	struct timespec end_time, *to = NULL;
	int ret;
	int err = -EFAULT, fdcount, len, size;
 	unsigned long todo ;
	struct poll_list *head;
 	struct poll_list *walk;
	s64 secs_to_sleep;
	s64 nsecs_to_sleep;
	s64 time_to_sleep = 0;
	int is_dialated = 0;
	struct poll_helper_struct helper;
	struct poll_helper_struct * poll_helper =  &helper;
	struct list_head *list;
    struct task_struct *taskRecurse;
	

	
	acquire_irq_lock(&current->dialation_lock,flags);
	if(experiment_stopped == RUNNING && current->virt_start_time != NOTSET && timeout_msecs >= 0 && atomic_read(&experiment_stopping) == 0){
	
		atomic_inc(&n_active_syscalls);
		is_dialated = 1;

		secs_to_sleep = timeout_msecs / MSEC_PER_SEC;
		nsecs_to_sleep = (timeout_msecs % MSEC_PER_SEC) * NSEC_PER_MSEC;
		time_to_sleep = (secs_to_sleep*1000000000) + nsecs_to_sleep;
		if(experiment_type != CS && time_to_sleep < expected_increase)
	        goto revert_poll;
		    
		if (nfds > RLIMIT_NOFILE){
			PDEBUG_E("Sys Poll: Poll Process Invalid");
			goto revert_poll;
		}


		poll_helper->head = (struct poll_list *) kmalloc(POLL_STACK_ALLOC/sizeof(long), GFP_KERNEL);
		if(poll_helper->head == NULL){
			PDEBUG_E("Sys Poll: Poll Process NOMEM");
			goto revert_poll;
		}
		
		poll_helper->table = (struct poll_wqueues *) kmalloc(sizeof(struct poll_wqueues), GFP_KERNEL);
		if(poll_helper->table == NULL){
			PDEBUG_E("Sys Poll: Poll Process NOMEM");
			kfree(poll_helper->head);
			goto revert_poll;
		}


		head = poll_helper->head;	
		poll_helper->err = -EFAULT;
		atomic_set(&poll_helper->done,0);
		poll_helper->walk = head;
		walk = head;
		init_waitqueue_head(&poll_helper->w_queue);


		len = (nfds < N_STACK_PPS ? nfds: N_STACK_PPS);
		todo = nfds;
		for (;;) {
			walk->next = NULL;
			walk->len = len;
			if (!len)
				break;

			if (copy_from_user(walk->entries, ufds + nfds-todo,
					sizeof(struct pollfd) * walk->len)) {
				kfree(head);
				kfree(poll_helper->table);		
				goto  revert_poll;
			}

			todo -= walk->len;
			if (!todo)
				break;

			len = (todo < POLLFD_PER_PAGE ? todo : POLLFD_PER_PAGE );
			size = sizeof(struct poll_list) + sizeof(struct pollfd) * len;
			walk = walk->next = kmalloc(size, GFP_KERNEL);
			if (!walk) {
				err = -ENOMEM;
				kfree(head);
				kfree(poll_helper->table);		
				goto revert_poll;
				
			}
		}
		poll_initwait(poll_helper->table);
		do_gettimeofday(&ktv);
		now = timeval_to_ns(&ktv);	
		now_new = get_dilated_time(current);
		s64 wakeup_time;
		wakeup_time = now_new + ((secs_to_sleep*1000000000) + nsecs_to_sleep)*Sim_time_scale; 
		PDEBUG_V("Sys Poll: Poll Process Waiting %d. Timeout sec %d, nsec %d.",current->pid,secs_to_sleep,nsecs_to_sleep);
		hmap_put_abs(&poll_process_lookup,current->pid,poll_helper);			
		release_irq_lock(&current->dialation_lock,flags);

		while(1){
            set_current_state(TASK_INTERRUPTIBLE);	
			if(now_new < wakeup_time){
			
			
				if(atomic_read(&poll_helper->done) != 0){
		            atomic_set(&poll_helper->done,0);	
				    err = do_dialated_poll(poll_helper->nfds, poll_helper->head,poll_helper->table,current);
				    if(err || poll_helper->err == FINISHED || atomic_read(&experiment_stopping) == 1){
					    poll_helper->err = err; 
					    break;
				    }
				}		
    			wait_event(poll_helper->w_queue,atomic_read(&poll_helper->done) != 0);    			
		        set_current_state(TASK_RUNNING);        
		        

			}
		
			
			now_new = get_dilated_time(current);
			if(now_new < wakeup_time) {
				acquire_irq_lock(&current->dialation_lock,flags);
			    if(current->freeze_time == 0 && atomic_read(&experiment_stopping) == 0) {
			        kill(current,SIGCONT,NULL);
					list_for_each(list, &current->children)
 		   			{
            			taskRecurse = list_entry(list, struct task_struct, sibling);     	
						if (taskRecurse != NULL && taskRecurse->pid == 0) {
							    continue;
						}
						//taskRecurse->freeze_time = 0;
						/* Let other children use the cpu */
						if(taskRecurse != NULL)
						kill(taskRecurse, SIGCONT, NULL); 
					}
				}				
			    release_irq_lock(&current->dialation_lock,flags);		
			}
			else{
			    poll_helper->err = 0;
			    break;
			}

		}
		
		acquire_irq_lock(&current->dialation_lock,flags);
		hmap_remove_abs(&poll_process_lookup, current->pid);
		release_irq_lock(&current->dialation_lock,flags);

		s64 diff = 0;
		if(wakeup_time > now_new){
			diff = wakeup_time - now_new; 
			PDEBUG_V("Sys Poll: Resumed Poll Process Early %d. Resume time = %llu. Difference = %llu\n",current->pid, now_new,diff );

		}
		else{
			diff = now_new - wakeup_time;
			PDEBUG_V("Sys Poll: Resumed Poll Process Expiry %d. Resume time = %llu. Difference = %llu\n",current->pid, now_new,diff );

		}
		
		poll_freewait(poll_helper->table);
		for (walk = head; walk; walk = walk->next) {
			struct pollfd *fds = walk->entries;
			int j;

			for (j = 0; j < walk->len; j++, ufds++)
				if (__put_user(fds[j].revents, &ufds->revents))
					goto out_fds;
		}

		err = poll_helper->err;

		out_fds:
		walk = head->next;
		while (walk) {
			struct poll_list *pos = walk;
			walk = walk->next;
			kfree(pos);
		}
		kfree(head);
		kfree(poll_helper->table);		
		PDEBUG_V("Sys Poll: Poll Process Finished %d",current->pid);
		atomic_dec(&n_active_syscalls);			
		return err;
		
		
		revert_poll:
		release_irq_lock(&current->dialation_lock,flags);	
		atomic_dec(&n_active_syscalls);
   			
   		return ref_sys_poll(ufds,nfds,timeout_msecs);

	}
	
   	release_irq_lock(&current->dialation_lock,flags);	
    return ref_sys_poll(ufds,nfds,timeout_msecs);
	



}


/***
Finds us the location of the system call table
***/
unsigned long **aquire_sys_call_table(void)
{
        unsigned long int offset = PAGE_OFFSET;
        unsigned long **sct;
        while (offset < ULLONG_MAX) {
                sct = (unsigned long **)offset;

                if (sct[__NR_close] == (unsigned long *) sys_close)
                        return sct;

                offset += sizeof(void *);
        }
        return NULL;
}

