#include "includes.h"
#include "module.h"
#include "utils.h"

// number of TRACERS in the experiment
extern int tracer_num;
// number of tracers for which a spinner has already been spawned
extern int n_processed_tracers;
extern int EXP_CPUS;
extern int TOTAL_CPUS;
// the main synchronization kernel thread for experiments
extern struct task_struct *round_task;
// flag to determine state of the experiment
extern int experiment_stopped;
extern int experiment_status;
extern int experiment_type;

extern struct mutex exp_lock;
extern int *per_cpu_chain_length;
extern llist * per_cpu_tracer_list;

extern hashmap poll_process_lookup;
extern hashmap select_process_lookup;
extern hashmap sleep_process_lookup;
//hashmap of <TRACER_NUMBER, TRACER_STRUCT>
extern hashmap get_tracer_by_id;
//hashmap of <TRACER_PID, TRACER_STRUCT>
extern hashmap get_tracer_by_pid;
extern atomic_t experiment_stopping;
extern wait_queue_head_t expstop_call_proc_wqueue;
extern unsigned long **sys_call_table;

extern int run_usermode_tracer_spin_process(char *path, char **argv,
        char **envp, int wait);
extern void signal_cpu_worker_resume(tracer * curr_tracer);
extern s64 get_dilated_time(struct task_struct * task);
extern int cleanup_experiment_components(void);


extern struct poll_list {
	struct poll_list *next;
	int len;
	struct pollfd entries[0];
};
extern struct poll_helper_struct;
extern hashmap poll_process_lookup;
extern atomic_t progress_n_rounds ;
extern atomic_t progress_n_enabled ;
extern atomic_t n_waiting_tracers;
extern wait_queue_head_t progress_sync_proc_wqueue;


extern asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp,
                                        struct timespec __user *rmtp);
extern asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds,
                                      unsigned int nfds, int timeout_msecs);
extern asmlinkage int (*ref_sys_select)(int n, fd_set __user *inp,
                                        fd_set __user *outp, fd_set __user *exp,
                                        struct timeval __user *tvp);
extern asmlinkage long (*ref_sys_clock_nanosleep)(const clockid_t which_clock,
        int flags, const struct timespec __user * rqtp,
        struct timespec __user * rmtp);
extern asmlinkage int (*ref_sys_clock_gettime)(const clockid_t which_clock,
        struct timespec __user * tp);
extern unsigned long orig_cr0;
extern s64 round_error;
extern s64 n_rounds;
extern s64 round_error_sq;
extern s64 expected_time;




/***
Remove head of schedule queue and return the task_struct of the head element.
Assumes tracer write lock is acquired prior to call.
***/
int pop_schedule_list(tracer * tracer_entry) {
	if (!tracer_entry)
		return 0;
	lxc_schedule_elem * head;
	head = llist_pop(&tracer_entry->schedule_queue);
	struct task_struct * curr_task;
	int pid;
	if (head != NULL) {
		pid = head->pid;
		hmap_remove_abs(&tracer_entry->valid_children, head->pid);
		kfree(head);
		return pid;
	}
	return 0;
}

/***
Requeue schedule queue, i.e pop from head and add to tail. Assumes tracer
write lock is acquired prior to call
***/
void requeue_schedule_list(tracer * tracer_entry) {
	if (tracer_entry == NULL)
		return;
	llist_requeue(&tracer_entry->schedule_queue);
}

/*
Assumes tracer write lock is acquired prior to call. Must return with lock
still held
*/
void clean_up_schedule_list(tracer * tracer_entry) {

	int pid = 1;
	struct pid *pid_struct;
	struct task_struct * task;

	while ( pid != 0) {
		pid = pop_schedule_list(tracer_entry);
		hmap_remove_abs(&tracer_entry->ignored_children, pid);
		if (pid) {
			pid_struct = find_get_pid(pid);
			task = pid_task(pid_struct, PIDTYPE_PID);
			if (task != NULL) {
				task->virt_start_time = 0;
				task->past_physical_time = 0;
				task->dilation_factor = 0;
				task->freeze_time = 0;
				task->past_virtual_time = 0;
				task->wakeup_time = 0;
			}
		} else
			break;
	}
}

/*
Assumes tracer read lock is acquired before function call
*/
int schedule_list_size(tracer * tracer_entry) {
	if (!tracer_entry)
		return 0;
	return llist_size(&tracer_entry->schedule_queue);
}



/*
Assumes tracer write lock is acquired prior to call. Must return with write
lock still acquired.
*/
void update_tracer_schedule_queue_elem(tracer * tracer_entry,
                                       struct task_struct * tracee) {

	lxc_schedule_elem * elem =
	    (lxc_schedule_elem *)hmap_get_abs(&tracer_entry->valid_children,
	                                      tracee->pid);
	// represents base time allotted to each process by the TimeKeeper scheduler.
	// Only useful in single core mode.
	s64 base_time_quanta;
	s64 base_quanta_n_insns = 0;
	s32 rem = 0;

	if (elem) {

#ifdef __TK_MULTI_CORE_MODE

		elem->n_insns_share = tracer_entry->quantum_n_insns;
		elem->n_insns_left = tracer_entry->quantum_n_insns;
		elem->n_insns_curr_round = 0;

#else
		if (tracee->static_prio <= 120) {
			base_time_quanta = (140 - tracee->static_prio) * 10000;
		} else {
			/* 200 us for now for all lower priority process. TODO */
			base_time_quanta = 200000;
		}

		base_quanta_n_insns =
		    div_s64_rem(tracer_entry->dilation_factor * base_time_quanta,
		                REF_CPU_SPEED, &rem);
		base_quanta_n_insns += rem;

		elem->n_insns_share = base_quanta_n_insns;
		PDEBUG_V("Update tracer schedule queue elem: "
		         "Pid: %d, n_insns_share: %d\n", elem->pid, elem->n_insns_share);
#endif

	}
}

/*
Assumes tracer write lock is acquired prior to call. Must return with lock still
acquired.
*/
void add_to_tracer_schedule_queue(tracer * tracer_entry,
                                  struct task_struct * tracee) {

	lxc_schedule_elem * new_elem;
	//represents base time allotted to each process by the TimeKeeper scheduler
	//Only useful in single core mode.
	s64 base_time_quanta;
	s64 base_quanta_n_insns = 0;
	s32 rem = 0;
	struct sched_param sp;

	if (!tracee ||
	        hmap_get_abs(&tracer_entry->ignored_children,
	                     tracee->pid) != NULL ||
	        hmap_get_abs(&tracer_entry->valid_children, tracee->pid) != NULL) {

		if (tracee &&
		        hmap_get_abs(&tracer_entry->ignored_children,
		                     tracee->pid) == NULL
		        && tracee != tracer_entry->spinner_task) {
			PDEBUG_V("Add to tracer schedule queue: "
			         "Tracer %d, tracee %d is already present. "
			         "Updating its attributes\n", tracer_entry->tracer_id,
			         tracee->pid);
			update_tracer_schedule_queue_elem(tracer_entry, tracee);
		} else if (tracee && tracee == tracer_entry->spinner_task ) {
			PDEBUG_A("Tracee spinner: %d ignored and not added to "
			         "Tracer: %d schedule queue\n", tracee->pid,
			         tracer_entry->tracer_id);
		} else if (tracee && tracee == tracer_entry->tracer_task) {
			PDEBUG_A("Tracee: %d ignored and not added to "
			         "Tracer: %d schedule queue\n", tracee->pid,
			         tracer_entry->tracer_id);
		} else {
			if (!tracee)
				PDEBUG_V("Add to tracer schedule queue: "
				         "Tracer %d, tracee is NULL \n",
				         tracer_entry->tracer_id);
		}

		return;
	}

	if (hmap_get_abs(&tracer_entry->ignored_children, tracee->pid) != NULL ) {
		PDEBUG_A("Tracee: %d ignored and not added to "
		         "Tracer: %d schedule queue\n", tracee->pid,
		         tracer_entry->tracer_id);
		return;
	}
	if (tracee && tracer_entry->spinner_task
	        && tracee->pid == tracer_entry->spinner_task->pid ) {
		PDEBUG_A("Tracee spinner: %d ignored and not added to "
		         "Tracer: %d schedule queue\n", tracee->pid,
		         tracer_entry->tracer_id);
		return ;
	} else if (tracee
	           && tracee == tracer_entry->tracer_task) {
		PDEBUG_A("Tracee: %d ignored and not added to "
		         "Tracer: %d schedule queue\n", tracee->pid,
		         tracer_entry->tracer_id);
		return ;
	}

	PDEBUG_I("Add to tracer schedule queue: "
	         "Tracer %d, Adding new tracee %d. \n",
	         tracer_entry->tracer_id, tracee->pid);

	new_elem =
	    (lxc_schedule_elem *)kmalloc(sizeof(lxc_schedule_elem), GFP_KERNEL);
	if (new_elem == NULL) {
		PDEBUG_E("Add to tracer schedule queue: "
		         "Tracer %d, tracee %d. Failed to alot Memory\n",
		         tracer_entry->tracer_id, tracee->pid);
		return;
	}

	new_elem->pid = tracee->pid;
	new_elem->curr_task = tracee;


#ifdef __TK_MULTI_CORE_MODE

	new_elem->n_insns_share = tracer_entry->quantum_n_insns;
	new_elem->n_insns_left = tracer_entry->quantum_n_insns;
	new_elem->n_insns_curr_round = 0;

#else
	if (tracee->static_prio <= 120) {
		base_time_quanta = (140 - tracee->static_prio) * 10000;
	} else {
		/* 200 us for now for all lower priority process. TODO */
		base_time_quanta = 200000;
	}

	base_quanta_n_insns =
	    div_s64_rem(tracer_entry->dilation_factor * base_time_quanta,
	                REF_CPU_SPEED, &rem);
	base_quanta_n_insns += rem;
	new_elem->n_insns_share = base_quanta_n_insns;
	new_elem->n_insns_left = base_quanta_n_insns;
	new_elem->n_insns_curr_round = 0;

#endif

	llist_append(&tracer_entry->schedule_queue, new_elem);


	bitmap_zero((&tracee->cpus_allowed)->bits, 8);
	cpumask_set_cpu(tracer_entry->cpu_assignment, &tracee->cpus_allowed);
	sp.sched_priority = 99;
	sched_setscheduler(tracee, SCHED_RR, &sp);
	hmap_put_abs(&tracer_entry->valid_children, new_elem->pid, new_elem);
	PDEBUG_I("Add to tracer schedule queue: "
	         "Tracer %d, tracee %d. Succeeded.\n", tracer_entry->tracer_id,
	         tracee->pid);
}


/*
Assumes tracer write lock is acquired prior to call. Must return with write
lock still acquired.
*/
void add_process_to_schedule_queue_recurse(tracer * tracer_entry,
        struct task_struct * tsk) {

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct task_struct *me;
	struct task_struct *t;

	if (!tsk || tsk->pid == 0 || !tracer_entry)
		return;

	// Do not add any of the threads of the tracer itself because
	// they are not dilated.
	me = tsk;
	t = me;

	do {

		if (tracer_entry->tracer_task->pid != t->pid)
			add_to_tracer_schedule_queue(tracer_entry, t);
	} while_each_thread(me, t);

	list_for_each(list, &tsk->children) {
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
			return;
		}
		add_process_to_schedule_queue_recurse(tracer_entry, taskRecurse);
	}
}

/*
Assumes tracer write lock is acquired prior to call. Must return with write
lock still acquired.
*/
void refresh_tracer_schedule_queue(tracer * tracer_entry) {
	if (tracer_entry) {
		add_process_to_schedule_queue_recurse(tracer_entry,
		                                      tracer_entry->tracer_task);
	}
}


/**
* write_buffer: <dilation_factor>,<tracer freeze_quantum>,<create_spinner>
**/
int register_tracer_process(char * write_buffer) {
	//Tracer will register itself by specifying the dilation factor
	u32 dilation_factor = 0;
	s64	tracer_freeze_quantum = 0;	//in ns
	//number of instructions per freeze quantum if REF_CPU_SPEED was used
	s64 tracer_ref_quantum_n_insns = 0;
	s32 rem = 0;
	tracer * new_tracer;
	uint32_t tracer_id;
	int i, nxt_idx;
	int best_cpu = 0;
	int should_create_spinner = 0;
	int spinner_pid = 0;
	struct task_struct * spinner_task = NULL;
	dilation_factor = atoi(write_buffer);
	nxt_idx = get_next_value(write_buffer);
	tracer_freeze_quantum = atoi(write_buffer + nxt_idx);
	nxt_idx = nxt_idx + get_next_value(write_buffer + nxt_idx);
	should_create_spinner = atoi(write_buffer + nxt_idx);


	if (experiment_status != INITIALIZED) {
		PDEBUG_E("Experiment must be initialized first "
		         "before tracer registration\n");
		return FAIL;
	}

	if (dilation_factor <= 0)
		dilation_factor = REF_CPU_SPEED;	//no dilation

	if (should_create_spinner > 0) {
		nxt_idx = nxt_idx + get_next_value(write_buffer + nxt_idx);
		spinner_pid = atoi(write_buffer + nxt_idx);
		should_create_spinner = 1;
		if (spinner_pid <= 0) {
			PDEBUG_E("Spinner pid must be greater than zero. "
			         "Received value: %d\n", spinner_pid);
			return FAIL;
		}

		spinner_task = find_task_by_pid(spinner_pid);

		if (!spinner_task) {
			PDEBUG_E("Spinner pid task not found. "
			         "Received pid: %d\n", spinner_pid);
			return FAIL;
		}

	} else
		should_create_spinner = 0;


	PDEBUG_I("Register Tracer: Starting ...\n");

	new_tracer = alloc_tracer_entry(tracer_id, dilation_factor);

	if (!new_tracer)
		return -ENOMEM;

	//freeze quantum must be at-least 1000 ns or 1 uS
	if (tracer_freeze_quantum < REF_CPU_SPEED )
		return -EFAULT;

	for (i = 0; i < EXP_CPUS; i++) {
		if (per_cpu_chain_length[i] < per_cpu_chain_length[best_cpu])
			best_cpu = i;
	}

	mutex_lock(&exp_lock);
	tracer_id = ++tracer_num;

	per_cpu_chain_length[best_cpu] ++;

	new_tracer->tracer_id = tracer_id;
	new_tracer->cpu_assignment = best_cpu + 2;
	new_tracer->tracer_task = current;
	new_tracer->freeze_quantum = tracer_freeze_quantum;

	tracer_ref_quantum_n_insns = tracer_freeze_quantum;
	new_tracer->quantum_n_insns =
	    div_s64_rem(new_tracer->dilation_factor * tracer_ref_quantum_n_insns,
	                REF_CPU_SPEED, &rem);
	new_tracer->quantum_n_insns += rem;
	new_tracer->create_spinner = should_create_spinner;

	hmap_put_abs(&get_tracer_by_id, tracer_id, new_tracer);
	hmap_put_abs(&get_tracer_by_pid, current->pid, new_tracer);
	llist_append(&per_cpu_tracer_list[best_cpu], new_tracer);

	mutex_unlock(&exp_lock);

	PDEBUG_I("Register Tracer: Pid: %d, ID: %d, dilation factor: %d, "
	         "freeze_quantum: %d, assigned cpu: %d, quantum_n_insns: %d. "
	         "Spinner pid = %d\n", current->pid, new_tracer->tracer_id,
	         new_tracer->dilation_factor, new_tracer->freeze_quantum,
	         new_tracer->cpu_assignment, new_tracer->quantum_n_insns,
	         spinner_pid);


	bitmap_zero((&current->cpus_allowed)->bits, 8);

	get_tracer_struct_write(new_tracer);
	cpumask_set_cpu(new_tracer->cpu_assignment, &current->cpus_allowed);
	if (should_create_spinner && spinner_task) {
		PDEBUG_I("Set Spinner Task for Tracer: %d, Spinner: %d\n",
		         current->pid, spinner_task->pid);
		new_tracer->spinner_task = spinner_task;
		kill_p(spinner_task, SIGSTOP);
		bitmap_zero((&spinner_task->cpus_allowed)->bits, 8);
		cpumask_set_cpu(1, &spinner_task->cpus_allowed);

	} else {
		new_tracer->spinner_task = NULL;
	}

	refresh_tracer_schedule_queue(new_tracer);
	put_tracer_struct_write(new_tracer);
	return new_tracer->cpu_assignment;	//return the allotted cpu back to the tracer.


}

/**
* write_buffer: <tracer_pid>,<dilation_factor>,<tracer freeze_quantum>
Assumes no tracer lock is acquired prior to call.
**/

int update_tracer_params(char * write_buffer) {

	u32 new_dilation_factor = 0;
	s64	new_tracer_freeze_quantum = 0;	//in ns
	//number of instructions per freeze quantum if REF_CPU_SPEED was used
	s64 tracer_ref_quantum_n_insns = 0;
	s32 rem = 0;
	tracer * tracer_entry;
	int i, nxt_idx;
	int best_cpu = 0;
	int tracer_pid = 0;
	struct task_struct * task = NULL;
	int found = 0;


	if (experiment_status != INITIALIZED) {
		PDEBUG_E("Experiment must be initialized first before "
		         "tracer update params\n");
		return FAIL;
	}



	tracer_pid = atoi(write_buffer);
	nxt_idx = get_next_value(write_buffer);
	new_dilation_factor = atoi(write_buffer + nxt_idx);
	nxt_idx = nxt_idx + get_next_value(write_buffer + nxt_idx);
	new_tracer_freeze_quantum = atoi(write_buffer + nxt_idx);

	if (new_dilation_factor <= 0)
		new_dilation_factor = REF_CPU_SPEED;	//no dilation

	//freeze quantum must be at-least 1000 ns or 1 uS
	if (new_tracer_freeze_quantum < REF_CPU_SPEED )
		return -EFAULT;

	if (experiment_stopped != RUNNING && experiment_stopped != FROZEN)
		return -EFAULT;


	for_each_process(task) {
		if (task != NULL) {
			if (task->pid == tracer_pid) {
				found = 1;
				break;
			}
		}
	}

	if (task && found) {
		tracer_entry = hmap_get_abs(&get_tracer_by_pid, task->pid);
	}

	if (tracer_entry) {
		get_tracer_struct_write(tracer_entry);
		tracer_entry->freeze_quantum = new_tracer_freeze_quantum;
		tracer_entry->dilation_factor = new_dilation_factor;

		tracer_ref_quantum_n_insns = new_tracer_freeze_quantum;
		tracer_entry->quantum_n_insns =
		    div_s64_rem(
		        tracer_entry->dilation_factor * tracer_ref_quantum_n_insns,
		        REF_CPU_SPEED, &rem);
		tracer_entry->quantum_n_insns += rem;
		refresh_tracer_schedule_queue(tracer_entry);

		PDEBUG_V("Updated params for tracer: %d, ID: %d, freeze_quantum: %d, "
		         "dilation_factor: %d, quantum_n_insns: %d\n", tracer_pid,
		         tracer_entry->tracer_id, tracer_entry->freeze_quantum,
		         tracer_entry->dilation_factor, tracer_entry->quantum_n_insns);
		put_tracer_struct_write(tracer_entry);
	}
	return SUCCESS;
}


/*
Assumes tracer write lock is acquired prior to call. Must return with write lock
still acquired.
*/
void update_task_virtual_time(tracer * tracer_entry,
                              struct task_struct * tsk, s64 n_insns_run) {

	s64 dilated_run_time = 0;
	s32 rem = 0;
	if (tracer_entry && tsk && n_insns_run > 0) {
		dilated_run_time = div_s64_rem(REF_CPU_SPEED * n_insns_run,
		                               tracer_entry->dilation_factor, &rem);
		dilated_run_time += rem;
		tsk->freeze_time =
		    tracer_entry->round_start_virt_time + dilated_run_time;

		//also update the spinner task's time
		if (tracer_entry->spinner_task && tsk != tracer_entry->spinner_task) {
			tracer_entry->spinner_task->freeze_time =
			    tracer_entry->round_start_virt_time + dilated_run_time;
		}
	}

}


/*
Assumes tracer write lock is acquired prior to call. Must return with write lock
still acquired.
*/
void update_all_children_virtual_time(tracer * tracer_entry) {

	s64 dilated_run_time;
	s64 curr_virtual_time;
	if (tracer_entry && tracer_entry->tracer_task) {
		dilated_run_time = tracer_entry->quantum_n_insns;

		if (tracer_entry->spinner_task)
			tracer_entry->spinner_task->freeze_time =
			    tracer_entry->round_start_virt_time  + dilated_run_time;

		curr_virtual_time =
		    tracer_entry->round_start_virt_time  + dilated_run_time;
		set_children_time(tracer_entry, tracer_entry->tracer_task,
		                  curr_virtual_time);
	}

}


/*
Assumes no tracer lock is acquired prior to call
*/
void update_all_tracers_virtual_time(int cpuID) {
	llist_elem * head;
	llist * tracer_list;
	tracer * curr_tracer;
	s64 err;
	int updated = 0;
	s64 advanced_time;
	s32 rem = 0;
	s64 quantum_n_insns;
	s64 overshot_n_insns;
	s64 undershot_n_insns;

	tracer_list =  &per_cpu_tracer_list[cpuID];
	head = tracer_list->head;


	while (head != NULL) {

		curr_tracer = (tracer*)head->item;
		get_tracer_struct_write(curr_tracer);
		if (curr_tracer->quantum_n_insns) {

			if (schedule_list_size(curr_tracer) == 0) {
				update_task_virtual_time(curr_tracer, curr_tracer->spinner_task,
				                         curr_tracer->quantum_n_insns);
			} else {
				update_all_children_virtual_time(curr_tracer);
			}

		}

		if (experiment_type == EXP_CBE) {

			if (cpuID == 0 && updated == 0) {
				expected_time = expected_time + curr_tracer->freeze_quantum;
				updated = 1;

			}

			//Quantum n insns contains amount of instructions actually run
			//in previous round.
			advanced_time = div_s64_rem(
			                    curr_tracer->quantum_n_insns * REF_CPU_SPEED,
			                    curr_tracer->dilation_factor, &rem);
			advanced_time = advanced_time + rem;

			curr_tracer->round_start_virt_time =
			    curr_tracer->round_start_virt_time + advanced_time;

			if (cpuID == 0) {
				init_task.freeze_time = curr_tracer->round_start_virt_time;
			} else if (
			    curr_tracer->round_start_virt_time < init_task.freeze_time) {
				init_task.freeze_time = curr_tracer->round_start_virt_time;
			}

			//Set quantum n insns for next round for the tracer.
			if (expected_time  + curr_tracer->freeze_quantum
			        < curr_tracer->round_start_virt_time) {
				curr_tracer->quantum_n_insns = 0; // for next round.
			} else if (expected_time < curr_tracer->round_start_virt_time) {
				err = curr_tracer->round_start_virt_time - expected_time;
				quantum_n_insns =
				    div_s64_rem(
				        curr_tracer->freeze_quantum * curr_tracer->dilation_factor,
				        REF_CPU_SPEED, &rem);
				quantum_n_insns = quantum_n_insns + rem;
				overshot_n_insns =
				    div_s64_rem(err * curr_tracer->dilation_factor,
				                REF_CPU_SPEED, &rem);
				overshot_n_insns = overshot_n_insns + rem;

				if (quantum_n_insns > overshot_n_insns) // should always be true
					curr_tracer->quantum_n_insns =
					    quantum_n_insns - overshot_n_insns;
				else {
					PDEBUG_E("Should never happen\n");
					curr_tracer->quantum_n_insns = quantum_n_insns;
				}
			} else if (expected_time > curr_tracer->round_start_virt_time) {
				err = expected_time - curr_tracer->round_start_virt_time;
				quantum_n_insns =
				    div_s64_rem(
				        curr_tracer->freeze_quantum * curr_tracer->dilation_factor,
				        REF_CPU_SPEED, &rem);
				quantum_n_insns = quantum_n_insns + rem;

				undershot_n_insns =
				    div_s64_rem(err * curr_tracer->dilation_factor,
				                REF_CPU_SPEED, &rem);
				undershot_n_insns = undershot_n_insns + rem;
				curr_tracer->quantum_n_insns = quantum_n_insns + undershot_n_insns;
			} else {
				quantum_n_insns =
				    div_s64_rem(
				        curr_tracer->freeze_quantum * curr_tracer->dilation_factor,
				        REF_CPU_SPEED, &rem);
				quantum_n_insns = quantum_n_insns + rem;
				curr_tracer->quantum_n_insns = quantum_n_insns;
			}

		} else {
			curr_tracer->round_start_virt_time =
			    curr_tracer->round_start_virt_time + curr_tracer->freeze_quantum;

			if (cpuID == 0) {
				init_task.freeze_time = curr_tracer->round_start_virt_time;
			} else if (curr_tracer->round_start_virt_time
			           < init_task.freeze_time) {
				init_task.freeze_time = curr_tracer->round_start_virt_time;
			}
		}
		put_tracer_struct_write(curr_tracer);
		head = head->next;
	}
}


/**
* write_buffer: result which indicates overflow number of instructions.
  It specifies the total number of instructions by which the tracer overshot
  in the current round. The overshoot is ignored if experiment type is CS.
  Assumes no tracer lock is acquired prior to call.
**/

int handle_tracer_results(char * buffer) {

	s64 result = 0 ;
	tracer * curr_tracer = hmap_get_abs(&get_tracer_by_pid, current->pid);
	int buf_len = strlen(buffer);
	int next_idx = 0;
	lxc_schedule_elem * curr_elem;
	struct pid *pid_struct;
	struct task_struct * task;
	s64 overshoot_time = 0;
	s32 rem = 0;
	s64 quantum_n_insns;

	if (!curr_tracer)
		return FAIL;

	if (experiment_type == EXP_CS) {
		signal_cpu_worker_resume(curr_tracer);
		return SUCCESS;
	}

	get_tracer_struct_write(curr_tracer);
	while (next_idx < buf_len) {
		result = atoi(buffer + next_idx);
		next_idx += get_next_value(buffer + next_idx);

		PDEBUG_V("Handle tracer results: Pid: %d, Tracer ID: %d, "
		         "Curr Result: %d, All results: %s\n", current->pid,
		         curr_tracer->tracer_id, result, buffer);


		if (result <= 0) {
			break;
		}

		if (result) { //result is a pid to be ignored
			PDEBUG_V("Handle tracer results: Pid: %d, Tracer ID: %d, "
			         "Ignoring Process: %d\n", current->pid,
			         curr_tracer->tracer_id, result);
			//pid_struct = find_get_pid(result);
			//task = pid_task(pid_struct, PIDTYPE_PID);

			hmap_put_abs(&curr_tracer->ignored_children, result, current);
		}
	}
	put_tracer_struct_write(curr_tracer);

	signal_cpu_worker_resume(curr_tracer);
	return SUCCESS;
}



int handle_stop_exp_cmd() {


	////set_current_state(TASK_INTERRUPTIBLE);
	atomic_set(&progress_n_enabled, 0);
	atomic_set(&progress_n_rounds, 0);
	atomic_set(&experiment_stopping, 1);
	wake_up_process(round_task);
	wake_up_interruptible(&progress_sync_proc_wqueue);
	wait_event_interruptible(expstop_call_proc_wqueue,
	                         atomic_read(&experiment_stopping) == 0
	                         && atomic_read(&n_waiting_tracers) == 0);


	PDEBUG_V("Returning from Stop Cmd\n");
	return cleanup_experiment_components();
}


/**
* write_buffer: <tracer_pid>,<network device name>
* Can be called after successfull synchronize and freeze command
**/
int handle_set_netdevice_owner_cmd(char * write_buffer) {


	char dev_name[IFNAMSIZ];
	int pid;
	struct pid * pid_struct = NULL;
	int i = 0;
	struct net * net;
	struct task_struct * task;
	tracer * curr_tracer;
	int found = 0;

	for (i = 0; i < IFNAMSIZ; i++)
		dev_name[i] = '\0';

	pid = atoi(write_buffer);
	int next_idx = get_next_value(write_buffer);


	for (i = 0; * (write_buffer + next_idx + i) != '\0'
	        && *(write_buffer + next_idx + i) != ','  && i < IFNAMSIZ ; i++)
		dev_name[i] = *(write_buffer + next_idx + i);

	PDEBUG_A("Set Net Device Owner: Received Pid: %d, Dev Name: %s\n",
	         pid, dev_name);

	struct net_device * dev;
	for_each_process(task) {
		if (task != NULL) {
			if (task->pid == pid) {
				pid_struct = get_task_pid(task, PIDTYPE_PID);
				found = 1;
				break;
			}
		}
	}


	if (task && found) {
		curr_tracer = hmap_get_abs(&get_tracer_by_pid, task->pid);
		if (!curr_tracer)
			return FAIL;

		get_tracer_struct_read(curr_tracer);
		task = curr_tracer->spinner_task;
		put_tracer_struct_read(curr_tracer);
		if (!task) {
			PDEBUG_E("Must have spinner task to "
			         "be able to set net device owner\n");
			return FAIL;
		}

		pid_struct = get_task_pid(task, PIDTYPE_PID);
		if (!pid_struct) {
			PDEBUG_E("Pid Struct of spinner task not found for tracer: %d\n",
			         curr_tracer->tracer_task->pid);
			return FAIL;
		}

		write_lock_bh(&dev_base_lock);
		for_each_net(net) {
			for_each_netdev(net, dev) {
				if (dev != NULL) {
					if (strcmp(dev->name, dev_name) == 0) {
						PDEBUG_A("Set Net Device Owner: "
						         "Found Specified Net Device: %s\n", dev_name);
						dev->owner_pid = pid_struct;
						found = 1;
					}
				}
			}
		}

		write_unlock_bh(&dev_base_lock);

	} else {
		return FAIL;
	}


	return SUCCESS;
}

/**
* write_buffer: <pid> of process
**/
int handle_gettimepid(char * write_buffer) {

	struct pid *pid_struct;
	struct task_struct * task;
	int pid;

	pid = atoi(write_buffer);

	PDEBUG_V("Handle gettimepid: Received Pid = %d\n", pid);

	for_each_process(task) {
		if (task != NULL) {
			if (task->pid == pid) {
				return get_dilated_time(task);
			}
		}
	}
	return FAIL;
}



/*** Wrappers for performing dialated poll and select system calls ***/

static inline unsigned int do_dialated_pollfd(struct pollfd *pollfd,
        poll_table *pwait, bool *can_busy_poll, unsigned int busy_flag,
        struct task_struct * tsk) {
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
				pwait->_key = pollfd->events | POLLERR | POLLHUP;
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

int do_dialated_poll(unsigned int nfds,  struct poll_list *list,
                     struct poll_wqueues *wait, struct task_struct * tsk) {
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
			                       busy_flag, tsk)) {
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


int max_sel_fd(unsigned long n, fd_set_bits *fds, struct task_struct * tsk) {
	unsigned long *open_fds;
	unsigned long set;
	int max;
	struct fdtable *fdt;

	/* handle last in-complete long-word first */
	set = ~(~0UL << (n & (BITS_PER_LONG - 1)));
	n /= BITS_PER_LONG;
	fdt = files_fdtable(current->files);
	open_fds = fdt->open_fds + n;

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

void wait_k_set(poll_table *wait, unsigned long in, unsigned long out,
                unsigned long bit, unsigned int ll_flag) {
	wait->_key = POLLEX_SET | ll_flag;
	if (in & bit)
		wait->_key |= POLLIN_SET;
	if (out & bit)
		wait->_key |= POLLOUT_SET;
}


int do_dialated_select(int n, fd_set_bits *fds, struct task_struct * tsk) {
	ktime_t expire, *to = NULL;
	struct poll_wqueues table;
	poll_table *wait;
	int retval, i, timed_out = 0;
	unsigned long slack = 0;
	unsigned int busy_flag = 0;
	unsigned long busy_end = 0;

	rcu_read_lock();
	retval = max_sel_fd(n, fds, tsk);
	rcu_read_unlock();

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
	return retval;
}
