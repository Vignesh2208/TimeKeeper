#include "includes.h"
#include "module.h"
#include "utils.h"


extern int tracer_num; 					// number of TRACERS in the experiment
extern int n_processed_tracers;			// number of tracers for which a spinner has already been spawned
extern int EXP_CPUS;
extern int TOTAL_CPUS;
extern struct task_struct *round_sync_task; 		// the main synchronization kernel thread for experiments
extern int experiment_stopped; 			 			// flag to determine state of the experiment
					 		
extern struct mutex exp_lock;
extern int *per_cpu_chain_length;
extern llist * per_cpu_tracer_list;

extern hashmap poll_process_lookup;
extern hashmap select_process_lookup;
extern hashmap sleep_process_lookup;
extern hashmap get_tracer_by_id;			//hashmap of <TRACER_NUMBER, TRACER_STRUCT> 
extern hashmap get_tracer_by_pid;			//hashmap of <TRACER_PID, TRACER_STRUCT>
extern atomic_t experiment_stopping;
extern wait_queue_head_t expstop_call_proc_wqueue;

extern int run_usermode_tracer_spin_process(char *path, char **argv, char **envp, int wait);
extern void signal_cpu_worker_resume(tracer * curr_tracer);
extern s64 get_dilated_time(struct task_struct * task);



/*** 
Remove head of schedule queue and return the task_struct of the head element 
***/
int pop_schedule_list(tracer * tracer_entry){
	if(tracer_entry == NULL)
		return 0;
	lxc_schedule_elem * head;
	head = llist_pop(&tracer_entry->schedule_queue);
	struct task_struct * curr_task;
	int pid;
	if(head != NULL){
		pid = head->pid;
		hmap_remove_abs(&tracer_entry->valid_children, head->pid);
		hmap_remove_abs(&tracer_entry->ignored_children, head->pid);
		kfree(head);
		return pid;
	}
	return 0;
}

/***
Get pointer to task_Struct of head but don't remove the element from the schedule list 
***/
lxc_schedule_elem * schedule_list_get_head(tracer * tracer_entry){
	if(tracer_entry == NULL) {
		PDEBUG_V("Get Head: TRACER ENTRY is Null\n");
		return NULL;
	}
	return llist_get(&tracer_entry->schedule_queue, 0);
}

/***
Requeue schedule queue, i.e pop from head and add to tail 
***/
void requeue_schedule_list(tracer * tracer_entry){
	if(tracer_entry == NULL)
		return;
	llist_requeue(&tracer_entry->schedule_queue);
}

void clean_up_schedule_list(tracer * tracer_entry){

	int pid = 1;
	struct pid *pid_struct;
	struct task_struct * task;

	
	
	while( pid != 0){
		pid = pop_schedule_list(tracer_entry);

		if(pid){
			pid_struct = find_get_pid(pid); 	 
			task = pid_task(pid_struct,PIDTYPE_PID); 
			if(task != NULL)
				kill(task, SIGKILL);
		}
		else
			break;
	}
}

int schedule_list_size(tracer * tracer_entry){
	if(tracer_entry == NULL)
		return 0;
	return llist_size(&tracer_entry->schedule_queue);
}




void update_tracer_schedule_queue_elem(tracer * tracer_entry, struct task_struct * tracee){

	lxc_schedule_elem * elem = hmap_get_abs(&tracer_entry->valid_children, tracee->pid);
	s64 base_time_quanta;	//represents base time allotted to each process by the TimeKeeper scheduler. Only useful in single core mode.
	s64 base_quanta_n_insns = 0;
	s32 rem = 0;

	if(elem){

		#ifdef __TK_MULTI_CORE_MODE

			elem->n_insns_share = tracer_entry->quantum_n_insns;
			elem->n_insns_left = tracer_entry->quantum_n_insns;
			elem->n_insns_curr_round = 0;

		#else
			if(tracee->static_prio <= 120){
	    		base_time_quanta = (140 - new_element->static_priority)* 10000;     		
	    	}
			else{
				/* 200 us for now for all lower priority process. TODO */		
		    	base_time_quanta = 200000; 
			}

			base_quanta_n_insns = div_s64_rem(tracer_entry->dilation_factor*base_time_quanta,REF_CPU_SPEED,&rem);
			base_quanta_n_insns += rem;


			elem->n_insns_share = base_quanta_n_insns;
			elem->n_insns_left = base_quanta_n_insns;
			elem->n_insns_curr_round = 0;
		#endif

	}
}

void add_to_tracer_schedule_queue(tracer * tracer_entry, struct task_struct * tracee){

	lxc_schedule_elem * new_elem;
	s64 base_time_quanta;	//represents base time allotted to each process by the TimeKeeper scheduler. Only useful in single core mode.
	s64 base_quanta_n_insns = 0;
	s32 rem = 0;
	struct sched_param sp;

	if(!tracee || hmap_get_abs(&tracer_entry->ignored_children, tracee->pid) != NULL || hmap_get_abs(&tracer_entry->valid_children, tracee->pid) != NULL){

		if(tracee && hmap_get_abs(&tracer_entry->valid_children, tracee->pid) != NULL){
			update_tracer_schedule_queue_elem(tracer_entry, tracee);
		}
		else if(tracee){
			PDEBUG_A("Tracee: %d ignored and not added to Tracer: %d schedule queue\n", tracee->pid, tracer_entry->tracer_id);
		}
	}

	new_elem = (lxc_schedule_elem *)kmalloc(sizeof(lxc_schedule_elem), GFP_KERNEL);
	if(new_elem == NULL)
		return;

	new_elem->pid = tracee->pid;
	new_elem->curr_task = tracee;


	#ifdef __TK_MULTI_CORE_MODE

		new_elem->n_insns_share = tracer_entry->quantum_n_insns;
		new_elem->n_insns_left = tracer_entry->quantum_n_insns;
		new_elem->n_insns_curr_round = 0;

	#else
		if(tracee->static_prio <= 120){
    		base_time_quanta = (140 - new_element->static_priority)* 10000;     		
    	}
		else{
			/* 200 us for now for all lower priority process. TODO */		
	    	base_time_quanta = 200000; 
		}

		base_quanta_n_insns = div_s64_rem(tracer_entry->dilation_factor*base_time_quanta,REF_CPU_SPEED,&rem);
		base_quanta_n_insns += rem;


		new_elem->n_insns_share = base_quanta_n_insns;
		new_elem->n_insns_left = base_quanta_n_insns;
		new_elem->n_insns_curr_round = 0;

	#endif


	bitmap_zero((&tracee->cpus_allowed)->bits, 8);
    cpumask_set_cpu(tracer_entry->cpu_assignment,&tracee->cpus_allowed);
	sp.sched_priority = 99;
	sched_setscheduler(tracee, SCHED_RR, &sp);
	hmap_put_abs(&tracer_entry->valid_children,new_elem->pid, new_elem);
	llist_append(&tracer_entry->schedule_queue, new_elem); 




}

void add_process_to_schedule_queue_recurse(tracer * tracer_entry, struct task_struct * tsk){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct task_struct *me;
    struct task_struct *t;

	if(tsk == NULL || tsk->pid == 0 || !tracer_entry)
		return;

	if(tracer_entry->tracer_task != tsk){
		// Do not add any of the threads of the tracer itself because they are not dilated.
		/* set policy for all threads as well */
		do {
			if (t->pid != aTask->pid) {
				add_to_tracer_schedule_queue(tracer_entry, t);
			}
		} while_each_thread(me, t);

	}

	list_for_each(list, &aTask->children)
	{
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
			return;
		}

		add_to_tracer_schedule_queue(tracer_entry, taskRecurse);
		add_process_to_schedule_queue_recurse(tracer_entry, taskRecurse);
	}

}

void refresh_tracer_schedule_queue(tracer * tracer_entry){
	if(tracer_entry){
		add_process_to_schedule_queue_recurse(tracer_entry, tracer_entry->tracer_task);
	}
}


/**
* write_buffer: <dilation_factor>,<tracer freeze_quantum>
**/
int register_tracer_process(char * write_buffer){
	//Tracer will register itself by specifying the dilation factor
	u32 dilation_factor = 0;
	s64	tracer_freeze_quantum = 0;	//in ns
	s64 tracer_ref_quantum_n_insns = 0;	//number of instructions per freeze quantum if REF_CPU_SPEED was used
	s32 rem = 0;
	tracer * new_tracer;
	uint32_t tracer_id;
	int i, nxt_idx;
	int best_cpu = 0;
	dilation_factor = atoi(write_buffer);
	nxt_idx = get_next_value(write_buffer);
	tracer_freeze_quantum = atoi(write_buffer + nxt_idx);

	if(experiment_status != INITIALIZED){
		PDEBUG_E("Experiment must be initialized first before tracer registration\n");
		return FAIL;	
	}

	if(dilation_factor <= 0)
		dilation_factor = REFERENCE_CPU_SPEED;	//no dilation


	mutex_lock(&exp_lock)
	tracer_id = ++tracer_num;
	mutex_unlock(&exp_lock)

	new_tracer = alloc_tracer_entry(tracer_id, dilation_factor)

	if(!new_tracer)
		return -ENOMEM;

	if(tracer_freeze_quantum < REF_CPU_SPEED ) //freeze quantum must be at-least 1000 ns or 1 uS
		return -EFAULT;

	for(i = 0; i < EXP_CPUS; i++){
		if(per_cpu_chain_length[i] < per_cpu_chain_length[best_cpu])
			i = best_cpu;
	}

	per_cpu_chain_length[best_cpu] ++;

	new_tracer->tracer_id = tracer_id;
	new_tracer->cpu_assignment = best_cpu + 2;
	new_tracer->tracer_task = current;
	new_tracer->freeze_quantum = tracer_freeze_quantum;

	tracer_ref_quantum_n_insns = tracer_freeze_quantum;
	new_tracer->quantum_n_insns = div_s64_rem(new_tracer->dilation_factor*tracer_ref_quantum_n_insns,REF_CPU_SPEED,&rem);
	new_tracer->quantum_n_insns += rem;

	mutex_lock(&exp_lock)
	hmap_put_abs(&get_tracer_by_id, tracer_id, new_tracer);
	hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
	llist_append(&per_cpu_tracer_list[best_cpu], new_tracer);
	mutext_unlock(&exp_lock);


	bitmap_zero((&current->cpus_allowed)->bits, 8);
   	cpumask_set_cpu(new_tracer->cpu_assignment,&current->cpus_allowed);

   	refresh_tracer_schedule_queue(new_tracer);



   	//need to create spinner task before returning

   	char *argv[] = { "/bin/x64_synchronizer", NULL };
	static char *envp[] = {
        	"HOME=/",
	        "TERM=linux",
        	"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
	run_usermode_tracer_spin_process(argv[0], argv, envp, UMH_NO_WAIT );

   	return new_tracer->cpu_assignment;	//return the allotted cpu back to the tracer.


}

/**
* write_buffer: <tracer_pid>,<dilation_factor>,<tracer freeze_quantum>
**/

int update_tracer_params(char * write_buffer){

	u32 new_dilation_factor = 0;
	s64	new_tracer_freeze_quantum = 0;	//in ns
	s64 tracer_ref_quantum_n_insns = 0;	//number of instructions per freeze quantum if REF_CPU_SPEED was used
	s32 rem = 0;
	tracer * tracer_entry;
	int i, nxt_idx;
	int best_cpu = 0;
	int tracer_pid = 0;
	struct task_struct * task = NULL;
	int found = 0;
	

	tracer_pid = atoi(write_buffer);
	nxt_idx = get_next_value(write_buffer);
	new_dilation_factor = atoi(write_buffer+nxt_idx);
	nxt_idx = get_next_value(write_buffer);
	new_tracer_freeze_quantum = atoi(write_buffer + nxt_idx);

	if(new_dilation_factor <= 0)
		new_dilation_factor = REFERENCE_CPU_SPEED;	//no dilation

	if(new_tracer_freeze_quantum < REF_CPU_SPEED ) //freeze quantum must be at-least 1000 ns or 1 uS
		return -EFAULT;

	if(experiment_stopped != RUNNING && experiment_stopped != FROZEN)
		return -EFAULT;


	for_each_process(task) {
	    if(task != NULL) {
	        if(task->pid == tracer_pid) {
	        	 found = 1;
	     	     break;
	        }
	    }
	}
   
   	if(task && found){
   		tracer_entry = hmap_get_abs(&get_tracer_by_pid, task->pid);
	}
	
	if(tracer_entry){
		tracer_entry->freeze_quantum = new_tracer_freeze_quantum;
		tracer_entry->dilation_factor = new_dilation_factor;

		tracer_ref_quantum_n_insns = new_tracer_freeze_quantum;
		tracer_entry->quantum_n_insns = div_s64_rem(tracer_entry->dilation_factor*tracer_ref_quantum_n_insns,REF_CPU_SPEED,&rem);
		tracer_entry->quantum_n_insns += rem;
		refresh_tracer_schedule_queue(tracer_entry);

		PDEBUG_V("Updated params for tracer: %d, ID: %d\n", tracer_pid, tracer_entry->tracer_id);

	
	}
	

	return SUCCESS;

}

void update_task_virtual_time(tracer * tracer_entry, struct task_struct * tsk, s64 n_insns_run){

	s64 dilated_run_time = 0;
	s32 rem = 0;
	if(tracer_entry && tsk && n_insns_run > 0){
		dilated_run_time = div_s64_rem(REF_CPU_SPEED*n_insns_run,tracer_entry->dilation_factor,&rem);
		dilated_run_time += rem;
		tsk->freeze_time = tracer_entry->round_start_virt_time + dilated_run_time;

		//also update the spinner task's time

		if(tracer_entry->spinner_task && tsk != tracer_entry->spinner_task){
			tracer_entry->spinner_task->freeze_time = tracer_entry->round_start_virt_time + dilated_run_time;
		}
	}

}


void update_all_children_virtual_time(tracer * tracer_entry){

	s64 dilated_run_time;
	s64 curr_virtual_time;
	if(tracer_entry){
		dilated_run_time = tracer_entry->freeze_quantum;
		tracer_entry->spinner_task->freeze_time = tracer_entry->round_start_virt_time  + dilated_run_time;
		curr_virtual_time = tracer_entry->round_start_virt_time  + dilated_run_time;
		set_children_time(tracer_entry, tracer_entry->tracer_task, curr_virtual_time);
	}

}


void update_all_tracers_virtual_time(int cpuID){
	llist_elem * head;
	llist * tracer_list;
	tracer * curr_tracer;


	tracer_list =  &per_cpu_tracer_list[cpuID];
	head = tracer_list->head;
	    

	while(head != NULL){

		curr_tracer = (tracer_entry *)head->item;

		if (schedule_list_size(curr_tracer) == 0) {			
			update_task_virtual_time(tracer_entry, tracer_entry->spinner_task, tracer_entry->quantum_n_insns);
		}
		else{
           	update_all_children_virtual_time(curr_tracer);

		}

		curr_tracer->round_start_virt_time = curr_tracer->round_start_virt_time + tracer_entry->freeze_quantum;
		head = head->next;
	}	
}


/**
* write_buffer: comma separated list of ignored pids or zero if none
**/

int handle_tracer_results(char * write_buffer){

	int result = 0 ;
	tracer * curr_tracer = hmap_get_abs(&get_tracer_by_pid, current->pid);
	int buf_len = strlen(write_buffer);
	int next_idx = 0;
	lxc_schedule_elem * curr_elem;
	struct pid *pid_struct;
	struct task_struct * task;

	if(!curr_tracer)
		return FAIL;

	while(next_idx < buf_len){
		result = atoi(buffer + next_idx);
		next_idx = get_next_value(buffer + next_idx);

		if(result <= 0)
			break;

		curr_elem = hmap_get_abs(&curr_tracer->valid_children, result);
		if(curr_elem){

			pid_struct = find_get_pid(curr_elem->pid); 	 
			task = pid_task(pid_struct,PIDTYPE_PID); 

			if(task != NULL)
				hmap_put_abs(&curr_tracer->ignored_children, result, task);
			hmap_remove_abs(&curr_tracer->valid_children, result);
		}

	}

	signal_cpu_worker_resume(curr_tracer);
	return SUCCESS;
}



int handle_stop_exp_cmd(){

	set_current_state(TASK_INTERRUPTIBLE);
	atomic_set(&experiment_stopping,1);
	wait_event_interruptible(expstop_call_proc_wqueue, atomic_read(&experiment_stopping) == 0);

	return cleanup_experiment_components();
}


/**
* write_buffer: <tracer_pid>,network device name
**/
int handle_set_netdevice_owner_cmd(char * write_buffer){


    char dev_name[IFNAMSIZ];
    int pid;
    struct pid * pid_struct = NULL;
    int i = 0;
	struct net * net;
	struct task_struct * task;
	tracer * curr_tracer;
	int found = 0;

	for(i = 0; i < IFNAMSIZ; i++)
		dev_name[i] = '\0';

    pid = atoi(write_buffer);
    int next_idx = get_next_value(write_buffer);
	

	for(i = 0; *(write_buffer + next_idx + i) != '\0' && i < IFNAMSIZ ; i++)
		dev_name[i] = *(write_buffer + next_idx + i);

    PDEBUG_A("Set Net Device Owner: Received Pid: %d, Dev Name: %s\n", pid, dev_name);

	struct net_device * dev;
	int found = 0;
	
	for_each_process(task) {
	    if(task != NULL) {
	        if(task->pid == pid) {
	       
	            pid_struct = get_task_pid(task,PIDTYPE_PID);
	            found = 1;
	            break;
	        }
	    }
	}
   
   	if(task && found){
   		curr_tracer = hmap_get_abs(&get_tracer_by_pid, task->pid);
   		if(!curr_tracer)
   			return FAIL;

   		task = curr_tracer->spinner_task;
   		pid_struct = get_task_pid(task,PIDTYPE_PID);
   		if(!pid_struct){
   			PDEBUG_E("Pid Struct of spinner task not found for tracer: %d\n", curr_tracer->tracer_task->pid);
   			return FAIL;
   		}

	    write_lock_bh(&dev_base_lock);
		for_each_net(net) {
			for_each_netdev(net,dev) {
				if(dev != NULL) {
					if(strcmp(dev->name,dev_name) == 0) {
						PDEBUG_A("Set Net Device Owner: Found Specified Net Device: %s\n", dev_name);
						dev->owner_pid = pid_struct;
						found = 1;
			    	}
				}
			}
		}	
			
		write_unlock_bh(&dev_base_lock);

	}
	else{
		return FAIL;
	}

	return SUCCESS;
}

/**
* write_buffer: <pid> of process
**/
int handle_getttimepid(char * write_buffer){

	struct pid *pid_struct;
	struct task_struct * task;
	int pid;

	pid = atoi(write_buffer);

	for_each_process(task) {
	    if(task != NULL) {
	        if(task->pid == pid) {
	        	 return get_dilated_time(task);
	        }
	    }
	}
	return FAIL;
}