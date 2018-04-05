#include "utils.h"
#include "module.h"

extern int experiment_stopped;
extern int tracer_num;
extern int schedule_list_size(tracer * tracer_entry);
extern hashmap get_tracer_by_id;
extern struct mutex exp_lock;


void flush_buffer(char * buf, int size){
	int i = 0;
	for(i =  0; i < size; i++)
		buf[i] = '\0';
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

tracer * alloc_tracer_entry(uint32_t tracer_id, u32 dilation_factor){

	tracer * new_tracer = NULL;

	new_tracer = (tracer *)kmalloc(sizeof(tracer), GFP_KERNEL);
	if(!new_tracer)
		return NULL;


	new_tracer->dilation_factor = dilation_factor;
	new_tracer->tracer_id = tracer_id;
	new_tracer->round_start_virt_time = 0;

	flush_buffer(new_tracer->run_q_buffer, BUF_MAX_SIZE);
	new_tracer->buf_tail_ptr = 0;
	llist_init(&new_tracer->schedule_queue);
	hmap_init(&new_tracer->valid_children,"int",0);
	hmap_init(&new_tracer->ignored_children,"int",0);
	init_waitqueue_head(&new_tracer->w_queue);

	mutex_init(&new_tracer->tracer_lock);

	atomic_set(&new_tracer->w_queue_control,1);

	new_tracer->last_run = NULL;

	return new_tracer;

}

void free_tracer_entry(tracer * tracer_entry){
	

	hmap_destroy(&tracer_entry->valid_children);
	hmap_destroy(&tracer_entry->ignored_children);
	llist_destroy(&tracer_entry->schedule_queue);

	kfree(tracer_entry);	

}


/***
Set the time dilation variables to be consistent with all children
***/
void set_children_cpu(struct task_struct *aTask, int cpu) {
	struct list_head *list;
	struct task_struct *taskRecurse;
	struct task_struct *me;
    struct task_struct *t;

	if (aTask == NULL) {
		PDEBUG_E("Set Children CPU: Task does not exist\n");
		return;
	}
	if (aTask->pid == 0) {
		return;
	}

	me = aTask;
	t = me;

	/* set policy for all threads as well */
	do {
		if (t->pid != aTask->pid) {
			if (cpu == -1) {

				/* allow all cpus */
				cpumask_setall(&t->cpus_allowed);
			}
			else {
				bitmap_zero((&t->cpus_allowed)->bits, 8);
       			cpumask_set_cpu(cpu,&t->cpus_allowed);
			}
		}
	} while_each_thread(me, t);

	list_for_each(list, &aTask->children)
	{
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
			return;
		}


		if (cpu == -1) {
			/* allow all cpus */
			cpumask_setall(&taskRecurse->cpus_allowed);
	    }
		else {
			bitmap_zero((&taskRecurse->cpus_allowed)->bits, 8);
			cpumask_set_cpu(cpu,&taskRecurse->cpus_allowed);
		}
		set_children_cpu(taskRecurse, cpu);
	}
}


/***
Set the time variables to be consistent with all children
***/
void set_children_time(tracer * tracer_entry, struct task_struct *aTask, s64 time) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct task_struct *me;
    struct task_struct *t;
	unsigned long flags;

	if (aTask == NULL) {
        PDEBUG_E("Set Children Time: Task does not exist\n");
        return;
    }
    if (aTask->pid == 0) {
        return;
    }
	me = aTask;
	t = me;

	//if(aTask->pid != tracer_entry->tracer_task->pid) {
		// do not set for any threads of tracer itself

		/* set it for all threads */
		do {

			if(t->pid != tracer_entry->tracer_task->pid){
			if (t->pid != aTask->pid) {
	           		t->virt_start_time = time;
	           		t->freeze_time = time;
	           		t->past_physical_time = 0;
	           		t->past_virtual_time = 0;
				if(experiment_stopped != RUNNING)
	     	       	t->wakeup_time = 0;
			}	

			}	
		} while_each_thread(me, t);

	//}

	list_for_each(list, &aTask->children)
	{
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
		        return;
		}
		taskRecurse->virt_start_time = time;
		taskRecurse->freeze_time = time;
		taskRecurse->past_physical_time = 0;
		taskRecurse->past_virtual_time = 0;
		if(experiment_stopped != RUNNING)
		    taskRecurse->wakeup_time = 0;
		set_children_time(tracer_entry, taskRecurse, time);
	}
}

void print_schedule_list(tracer* tracer_entry)
{
	int i = 0;
	lxc_schedule_elem * curr;
	if(tracer_entry != NULL) {
		for(i = 0; i < schedule_list_size(tracer_entry); i++){
			curr = llist_get(&tracer_entry->schedule_queue, i);
			if(curr != NULL) {
				PDEBUG_V("Schedule List Item No: %d, TRACER PID: %d, TRACEE PID: %d, N_insns_curr_round: %d, N_insns_left: %d, Size OF SCHEDULE QUEUE: %d\n",i, tracer_entry->tracer_task->pid,curr->pid, curr->n_insns_curr_round, curr->n_insns_left, schedule_list_size(tracer_entry));
			}
		}	
	}
}


/***
My implementation of the kill system call. Will send a signal to a container. Used for freezing/unfreezing containers
***/
int kill_p(struct task_struct *killTask, int sig) {
        struct siginfo info;
        int returnVal;
        info.si_signo = sig;
        info.si_errno = 0;
        info.si_code = SI_USER;
        if(killTask){
	        if ((returnVal = send_sig_info(sig, &info, killTask)) != 0)
	        {      
				PDEBUG_E("Kill: Error sending kill msg for pid %d\n", killTask->pid);      
	        }
    	}
        return returnVal;
}


tracer * get_tracer_for_task(struct task_struct * aTask){

	int i = 0;
	tracer * curr_tracer;
	if(!aTask)
		return NULL;

	if(experiment_stopped != RUNNING)
		return NULL;

	mutex_lock(&exp_lock);
	for(i = 1; i <= tracer_num; i++){

		curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
		if(curr_tracer){
			if(hmap_get_abs(&curr_tracer->valid_children, aTask->pid)){
					mutex_unlock(&exp_lock);
					return curr_tracer;
			}
		}

	}
	mutex_unlock(&exp_lock);

	return NULL;
}

int is_tracer_task(struct task_struct * aTask){
	int i = 0;
	tracer * curr_tracer;
	if(!aTask)
		return 0;

	if(experiment_stopped != RUNNING)
		return 0;

	mutex_lock(&exp_lock);
	for(i = 1; i <= tracer_num; i++){

		curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
		if(curr_tracer){
			if(curr_tracer->tracer_task == aTask){
					mutex_unlock(&exp_lock);
					return 1;
			}
		}

	}
	mutex_unlock(&exp_lock);

	return -1;
}