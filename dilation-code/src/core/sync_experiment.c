#include "dilation_module.h"

/*
Contains most of the functions in dealing with keeping an experiment synchronized within CORE.
Basic Flow of Functions for a synchronized experiment:
	-For every container, add_to_exp_proc is called, which calls add_to_exp, when passed the PID of the container,
		initializes the container and assigns it to a CPU, sched_priority and so forth
	-When all containers have been 'added', core_sync_exp is called, which will start the experiment.
	-Each container will run for its specified time, when its time is up, the timer is triggered, and exp_hrtimer_callback is called, which will call freeze_proc_exp_recurse to freeze the container, and unfreeze_proc_exp_recurse to unfreeze the next container
	-When the last containers timer is triggered, the catchup_func is woken up, and the synchronization phase begins.
	-At the end of the catchup_func, it will restart the round, and call unfreeze_proc_exp_recurse on the appropriate containers
	-When you want to end the experiment, set_clean_exp is called, telling catchup_func to stop at the end of the current round
	-When the end of the round is reached, catchup_func will call clean_exp and stop the experiment.
*/

void calcExpectedIncrease(void);
void calcTaskRuntime(struct dilation_task_struct * task);
struct dilation_task_struct * getNextRunnableTask(struct dilation_task_struct * task);
int catchup_func(void *data);
int calculate_sync_drift(void *data);
enum hrtimer_restart exp_hrtimer_callback( struct hrtimer *timer);
enum hrtimer_restart alt_hrtimer_callback( struct hrtimer * timer );
void add_to_exp(int pid);
void addToChain(struct dilation_task_struct *task);
void assign_to_cpu(struct dilation_task_struct *task);
void printChainInfo(void);
void add_to_exp_proc(char *write_buffer);
void clean_exp(void);
void set_clean_exp(void);
void set_cbe_exp_timeslice(char *write_buffer);
void set_children_time(struct task_struct *aTask, s64 time);
int freeze_children(struct task_struct *aTask, s64 time);
int unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time,struct dilation_task_struct *lxc);
int resume_all(struct task_struct *aTask,struct dilation_task_struct * lxc) ;
//int unfreeze_each_process(struct task_struct *aTask, s64 now, s64 expected_time, s64 running_time, struct hrtimer * task_timer, int CPUID, s64 head_past_virtual_time);

int freeze_proc_exp_recurse(struct dilation_task_struct *aTask);
int unfreeze_proc_exp_recurse(struct dilation_task_struct *aTask, s64 expected_time);
void core_sync_exp(void);
void set_children_policy(struct task_struct *aTask, int policy, int priority);
void set_children_cpu(struct task_struct *aTask, int cpu);
void add_sim_to_exp_proc(char *write_buffer);
void clean_stopped_containers(void);
void dilate_proc_recurse_exp(int pid, int new_dilation);
void change_containers_dilation(void);
void sync_and_freeze(void);
void calculate_virtual_time_difference(struct dilation_task_struct* task, s64 now, s64 expected_time);
s64 calculate_change(struct dilation_task_struct* task, s64 virt_time, s64 expected_time);
s64 get_virtual_time(struct dilation_task_struct* task, s64 now);

s64 PRECISION = 1000;   // doing floating point division in the kernel is HARD, therefore, I convert fractions to negative numbers.
						// The Precision specifies how far I scale the number: aka a TDF of 2 is converted to 2000,
s64 FREEZE_QUANTUM = 300000000;

s64 Sim_time_scale = 1;
hashmap poll_process_lookup;
hashmap select_process_lookup;
hashmap sleep_process_lookup;

extern struct poll_list {
    struct poll_list *next;
    int len;
    struct pollfd entries[0];
};
extern int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait,struct task_struct * tsk);
extern int do_dialated_select(int n, fd_set_bits *fds,struct task_struct * tsk);

int proc_num = 0; //the number of containers in the experiment
struct task_struct *catchup_task; //the task that calls the synchronization function (catchup_func)
struct dilation_task_struct *leader_task; // the leader task, aka the task with the highest dilation factor
int exp_highest_dilation = -100000000; //represents the highest dilation factor in the experiment (aka the TDF of the leader)
struct list_head exp_list; //the linked list of all containers in the experiment
struct mutex exp_mutex; //the mutex for exp_list to ensure we do not have race conditions
int experiment_stopped; // if == -1 then the experiment is not started yet, if == 0 then the experiment is currently running, if == 1 then the experiment is set to be stopped at the end of the current round.
struct dilation_task_struct* chainhead[EXP_CPUS]; //every CPU has a linked list of containers assinged to that CPU. This variable specifies the 'head' container for each CPU
s64 chainlength[EXP_CPUS]; //for every cpu, this value represents how long every container for that CPU will need to run in each round (TimeKeeper will assign a new container to the CPU with the lowest value)
struct task_struct* chaintask[EXP_CPUS];
int values[EXP_CPUS];
s64 actual_time; // The virtual time that every container should be at (or at least close to) at the end of every round
int number_of_heads = 0; //specifies how many head containers are in the experiment. This number will most often be equal to EXP_CPUS. Handles the special case if containers < EXP_CPUS so we do not have an array index out of bounds error
s64 expected_increase; // How much virtual time should increase at each round
int dilation_change = 0; //flag if dilation of a container has changed in the experiment. 0 means no changes, 1 means change
int experiment_type = NOTSET; // CBE for ns-3/core, CS for S3F (CS)
int stopped_change = 0;

// synchronization variables to support parallelization
static DECLARE_WAIT_QUEUE_HEAD(wq);
atomic_t worker_count = ATOMIC_INIT(0);
atomic_t running_done = ATOMIC_INIT(0);
atomic_t start_count = ATOMIC_INIT(0);
atomic_t catchup_Task_finished = ATOMIC_INIT(0);  // *** Added new
atomic_t woke_up_catchup_Task = ATOMIC_INIT(0);	
atomic_t wake_up_signal[EXP_CPUS];
atomic_t wake_up_signal_sync_drift[EXP_CPUS];
static int curr_process_finished_flag[EXP_CPUS];
static int curr_sync_task_finished_flag[EXP_CPUS];
static wait_queue_head_t per_cpu_wait_queue[EXP_CPUS];
static wait_queue_head_t per_cpu_sync_task_queue[EXP_CPUS];


extern struct task_struct *loop_task;
extern int TOTAL_CPUS;
extern struct timeline* timelineHead[EXP_CPUS];

extern void perform_on_children(struct task_struct *aTask, void(*action)(int,int), int val);
extern void change_dilation(int pid, int new_dilation);
extern s64 get_virtual_time_task(struct task_struct* task, s64 now);
extern asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
extern asmlinkage long (*ref_sys_select)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
extern unsigned long **sys_call_table; //address of the sys_call_table, so we can hijack certain system calls
unsigned long orig_cr0;


/*
Changes the FREEZE_QUANTUM for a CBE experiment
*/
void set_cbe_exp_timeslice(char *write_buffer){

	s64 timeslice;
	timeslice = atoi(write_buffer);
	if(experiment_type == CBE){
		printk(KERN_INFO "TimeKeeper : Set CBE Exp Timeslice: Got Freeze Quantum : %d\n", timeslice);
		FREEZE_QUANTUM = timeslice;
		FREEZE_QUANTUM = FREEZE_QUANTUM * 1000000 * Sim_time_scale;

		printk(KERN_INFO "TimeKeeper : Set CBE Exp Timeslice: Set Freeze Quantum : %lld\n", FREEZE_QUANTUM);
	}

}


/***
Adds the simulator pid to the experiment - might be deprecated.
***/
void add_sim_to_exp_proc(char *write_buffer) {
	int pid;
    pid = atoi(write_buffer);
	add_to_exp(pid);
}

/*
Creates and initializes dilation_task_struct given a task_struct.
*/
struct dilation_task_struct* initialize_node(struct task_struct* aTask) {
	struct dilation_task_struct* list_node;
	printk(KERN_INFO "TimeKeeper: Initialize Node: Adding a pid: %d Num of Nodes added to experiment so Far: %d, Number of Heads %d\n", aTask->pid, proc_num, number_of_heads);
	list_node = (struct dilation_task_struct *)kmalloc(sizeof(struct dilation_task_struct), GFP_KERNEL);
	list_node->linux_task = aTask;
	list_node->stopped = 0;
	list_node->next = NULL;
	list_node->prev = NULL;
	list_node->wake_up_time = 0;
	list_node->newDilation = -1;
	list_node->increment = 0;
	list_node->cpu_assignment = -1;
	list_node->rr_run_time = 0;
	
	list_node->last_run = NULL;
	llist_init(&list_node->schedule_queue);
	hmap_init(&list_node->valid_children,"int",0);
	hrtimer_init( &list_node->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hrtimer_init( &list_node->schedule_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	return list_node;
}

/***
wake_up_process returns 1 if it was stopped and gets woken up, 0 if it is dead OR already running. Used with CS experiments.
***/
void progress_exp(void) {
        wake_up_process(catchup_task);
}

/***
Reads a PID from the buffer, and adds the corresponding task to the experiment (i believe this does not support the adding
of processes if the experiment has alreaded started)
***/
void add_to_exp_proc(char *write_buffer) {
    int pid;
    pid = atoi(write_buffer);

	if (experiment_type == CS) {
		printk(KERN_INFO "TimeKeeper: Add To Exp Proc: Trying to add to wrong experiment type.. exiting\n");
	}
	else if (experiment_stopped == NOTRUNNING) {
        add_to_exp(pid);
	}
	else {
		printk(KERN_INFO "TimeKeeper: Add to Exp Proc: Trying to add a LXC to experiment that is already running\n");
	}
}

/***
Gets called by add_to_exp_proc(). Initiazes a containers timer, sets scheduling policy.
***/
void add_to_exp(int pid) {
        struct task_struct* aTask;
        struct dilation_task_struct* list_node;
        aTask = find_task_by_pid(pid);

        /* maybe I should just skip this pid instead of completely dropping out? */
        if (aTask == NULL)
        {
                printk(KERN_INFO "TimeKeeper: Add to Exp: Pid %d is invalid, Dropping out\n",pid);
                return;
        }

        proc_num++;
		experiment_type = CBE;
        if (EXP_CPUS < proc_num)
                number_of_heads = EXP_CPUS;
        else
                number_of_heads = proc_num;

		list_node = initialize_node(aTask);
        list_node->timer.function = &exp_hrtimer_callback;

        mutex_lock(&exp_mutex);
        list_add(&(list_node->list), &exp_list);
        mutex_unlock(&exp_mutex);
        if (exp_highest_dilation < list_node->linux_task->dilation_factor)
        {
                exp_highest_dilation = list_node->linux_task->dilation_factor;
                leader_task = list_node;
        }
}

/*
Sets all nodes added to the experiment to the same point in time, and freezes them
*/
void sync_and_freeze() {
	struct timeval now_timeval;
	s64 now;
	struct dilation_task_struct* list_node;
	struct list_head *pos;
	struct list_head *n;
	int i;
	int j;
	struct sched_param sp;
	int placed_lxcs;
	placed_lxcs = 0;
	unsigned long flags;

	printk(KERN_INFO "TimeKeeper: Sync And Freeze: ** Starting Experiment Synchronization **\n");

	if (proc_num == 0) {
		printk(KERN_INFO "TimeKeeper: Sync And Freeze: Nothing added to experiment, dropping out\n");
		return;
	}

	if (experiment_stopped != NOTRUNNING) {
        printk(KERN_INFO "TimeKeeper: Sync And Freeze: Trying to StartExp when an experiment is already running!\n");
        return;
    }


	hmap_init( &poll_process_lookup,"int",0);
	hmap_init( &select_process_lookup,"int",0);
	hmap_init( &sleep_process_lookup,"int",0);


	orig_cr0 = read_cr0();
	write_cr0(orig_cr0 & ~0x00010000);
	sys_call_table[__NR_select_dialated] = (unsigned long *)sys_select_new;	
	sys_call_table[__NR_poll] = (unsigned long *) sys_poll_new;
	write_cr0(orig_cr0);


	for (j = 0; j < number_of_heads; j++) {
        values[j] = j;
	}
    
	sp.sched_priority = 1;

	/* Create the threads for parallel computing */
	for (i = 0; i < number_of_heads; i++)
	{
		printk(KERN_INFO "TimeKeeper: Sync And Freeze: Adding Worker Thread %d\n", i);
		chainhead[i] = NULL;
		chainlength[i] = 0;
		if (experiment_type == CBE){
			init_waitqueue_head(&per_cpu_sync_task_queue[i]);
			curr_sync_task_finished_flag[i] = 0;
			chaintask[i] = kthread_run(&calculate_sync_drift, &values[i], "worker");

		}
	}

	/* If in CBE mode, find the leader task (highest TDF) */
	if (experiment_type == CBE) {
		list_for_each_safe(pos, n, &exp_list)
        	{
        		list_node = list_entry(pos, struct dilation_task_struct, list);
				if (list_node->linux_task->dilation_factor > exp_highest_dilation) {
                       		leader_task = list_node;
                        	exp_highest_dilation = list_node->linux_task->dilation_factor;
               	}
		}
	}

	/* calculate how far virtual time should advance every round */
    calcExpectedIncrease(); 

    do_gettimeofday(&now_timeval);
    now = timeval_to_ns(&now_timeval);
    actual_time = now;
	printk(KERN_INFO "TimeKeeper: Sync And Freeze: Setting the virtual start time of all tasks to be: %lld\n", actual_time);

    /* for every container in the experiment, set the virtual_start_time (so it starts at the same time), calculate
    how long each task should be allowed to run in each round, and freeze the container */
    list_for_each_safe(pos, n, &exp_list)
    {
        list_node = list_entry(pos, struct dilation_task_struct, list);
        if (experiment_type == CBE)
			calcTaskRuntime(list_node);

		/* consistent time */
        list_node->linux_task->virt_start_time = now; 
		if (experiment_type == CS) {
			list_node->expected_time = now;
			list_node->running_time = 0;
		}

	spin_lock_irqsave(&list_node->linux_task->dialation_lock,flags);
    list_node->linux_task->past_physical_time = 0;
	list_node->linux_task->past_virtual_time = 0;
	list_node->linux_task->wakeup_time = 0;
    spin_unlock_irqrestore(&list_node->linux_task->dialation_lock,flags);

	/* freeze all children */
	freeze_proc_exp_recurse(list_node); 

	spin_lock_irqsave(&list_node->linux_task->dialation_lock,flags);
	list_node->linux_task->freeze_time = now;
	spin_unlock_irqrestore(&list_node->linux_task->dialation_lock,flags);

		/* set priority and scheduling policy */
        if (list_node->stopped == -1) {
            printk(KERN_INFO "TimeKeeper: Sync And Freeze: One of the LXCs no longer exist.. exiting experiment\n");
            clean_exp();
            return;
        }

       	if (sched_setscheduler(list_node->linux_task, SCHED_RR, &sp) == -1 )
           	printk(KERN_INFO "TimeKeeper: Sync And Freeze: Error setting SCHED_RR %d\n",list_node->linux_task->pid);
        	set_children_time(list_node->linux_task, now);
			set_children_policy(list_node->linux_task, SCHED_RR, 1);

		if (experiment_type == CS) {
				printk(KERN_INFO "TimeKeeper: Sync And Freeze: Cpus allowed! : %d \n", list_node->linux_task->cpus_allowed);
     			bitmap_zero((&list_node->linux_task->cpus_allowed)->bits, 8);
	        	cpumask_set_cpu(list_node->cpu_assignment, &list_node->linux_task->cpus_allowed);
				set_children_cpu(list_node->linux_task, list_node->cpu_assignment);
		}

		printk(KERN_INFO "TimeKeeper: Sync And Freeze: Task running time: %lld\n", list_node->running_time);
	}

	/* If in CBE mode, assign all tasks to a specfic CPU (this has already been done if in CS mode) */
	if (experiment_type == CBE) {
		while (placed_lxcs < proc_num) {
			int highest_tdf;
			struct dilation_task_struct* task_to_assign;
			highest_tdf = -100000000;
			task_to_assign = NULL;
			list_for_each_safe(pos, n, &exp_list)
			{
				list_node = list_entry(pos, struct dilation_task_struct, list);
				if (list_node->cpu_assignment == -1 && list_node->linux_task->dilation_factor > highest_tdf) {
					task_to_assign = list_node;
					highest_tdf = list_node->linux_task->dilation_factor;
				}
			}
			if (task_to_assign->linux_task->dilation_factor > exp_highest_dilation) {
				leader_task = task_to_assign;
				exp_highest_dilation = task_to_assign->linux_task->dilation_factor;
			}
			assign_to_cpu(task_to_assign);
			placed_lxcs++;
		}
	}
    printChainInfo();

	/* Set what mode experiment is in, depending on experiment_type (CBE or CS) */
	if (experiment_type == CS)
		experiment_stopped = RUNNING;
	else
		experiment_stopped = FROZEN;

//if its 64-bit, start the busy loop task to fix the weird bug
#ifdef __x86_64
	kill(loop_task, SIGCONT, NULL);
#endif
	printk(KERN_INFO "TimeKeeper: Finished Sync and Freeze\n");

}

/***
Specifies the start of the experiment (if in CBE mode)
***/
void core_sync_exp() {
	struct dilation_task_struct* list_node;
	int i;
	ktime_t ktime;

	if (experiment_type == CS) {
		printk(KERN_INFO "TimeKeeper: Core Sync Exp: Trying to start wrong type of experiment.. exiting\n");
		return;
	}
	if (experiment_stopped != FROZEN) {
		printk(KERN_INFO "TimeKeeper: Core Sync Exp: Experiment is not ready to commence, must run synchronizeAndFreeze\n");
		return;
	}

	/* for every 'head' container, unfreeze it and set its timer to fire at some point in the future (based off running_time) */
	for (i=0; i<number_of_heads; i++)
	{
	    list_node = chainhead[i];
	 	freeze_proc_exp_recurse(list_node);

	}

	printk(KERN_INFO "TimeKeeper : Core Sync Exp: Waking up catchup task\n");
	experiment_stopped = RUNNING;
	wake_up_process(catchup_task);
}

/***
If we know the process with the highest TDF in the experiment, we can calculate how far it should progress in virtual time,
and set the global variable 'expected_increase' accordingly.
***/
void calcExpectedIncrease() {
        s32 rem;
        if (exp_highest_dilation > 0)
        {
                expected_increase = div_s64_rem(FREEZE_QUANTUM*PRECISION,exp_highest_dilation,&rem);
                expected_increase += rem;
        }
        else if (exp_highest_dilation == 0)
                expected_increase = FREEZE_QUANTUM;
        else {
                expected_increase = div_s64_rem(FREEZE_QUANTUM*(exp_highest_dilation*-1), PRECISION, &rem);
        }
        return;
}

/***
Given a task with a TDF, determine how long it should be allowed to run in each round, stored in running_time field
***/
void calcTaskRuntime(struct dilation_task_struct * task) {
        int dil;
        s64 temp_proc;
        s64 temp_high;
        s32 rem;

        /* temp_proc and temp_high are temporary dilations for the container and leader respectively.
        this is done just to make sure the Math works (no divide by 0 errors if the TDF is 0, by making the temp TDF 1) */
        temp_proc = 0;
        temp_high = 0;
        if (exp_highest_dilation == 0)
                temp_high = 1;
        else if (exp_highest_dilation < 0)
                temp_high = exp_highest_dilation*-1;

        dil = task->linux_task->dilation_factor;

        if (dil == 0)
                temp_proc = 1;
        else if (dil < 0)
                temp_proc = dil*-1;
        if (exp_highest_dilation == dil) {
        		/* if the leaders' TDF and the containers TDF are the same, let it run for the full amount (do not need to scale) */
                task->running_time = FREEZE_QUANTUM;
        }
        else if (exp_highest_dilation > 0 && dil > 0) {
        		/* if both the leaders' TDF and the containers TDF are > 0, scale running time */
                task->running_time = (div_s64_rem(FREEZE_QUANTUM,exp_highest_dilation,&rem) + rem)*dil;
        }
        else if (exp_highest_dilation > 0 && dil == 0) {
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION,exp_highest_dilation,&rem) + rem);
		}
        else if (exp_highest_dilation > 0 && dil < 0) { 
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION*PRECISION,exp_highest_dilation*temp_proc,&rem) + rem);

        }
        else if (exp_highest_dilation == 0 && dil < 0) { 
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION,temp_proc,&rem) + rem);
        }
        else if (exp_highest_dilation < 0 && dil < 0){ 
                task->running_time = (div_s64_rem(FREEZE_QUANTUM,temp_proc,&rem) + rem)*temp_high;
        }
	else {
		printk("TimeKeeper: Calc Task Runtime: Should be fixed when highest dilation is updated\n");
	}
        return;
}

/***
Function that determines what CPU a particular task should be assigned to. It simply finds the current CPU with the
smallest aggregated running time of all currently assigned containers. All containers that are assigned to the same
CPU are connected as a list, hence I call it a 'chain'
***/
void assign_to_cpu(struct dilation_task_struct* task) {
	int i;
	int index;
	s64 min;
	struct dilation_task_struct *walk;
	index = 0;
	min = chainlength[index];

	for (i=1; i<number_of_heads; i++)
	{
	    if (chainlength[i] < min)
	    {
		    min = chainlength[i];
		    index = i;
	    }
	}

	walk = chainhead[index];
	if (walk == NULL) {
		chainhead[index] = task;
		init_waitqueue_head(&per_cpu_wait_queue[index]);	
		curr_process_finished_flag[index] = 0;			
		atomic_set(&wake_up_signal_sync_drift[index],0);	
	}
	else {

		/* create doubly linked list */
		while (walk->next != NULL)
		{
			walk = walk->next;
		}
		walk->next = task;
		task->prev = walk; 
	}


	/* set CPU mask */
	chainlength[index] = chainlength[index] + task->running_time;
   	bitmap_zero((&task->linux_task->cpus_allowed)->bits, 8);
    cpumask_set_cpu(index+(TOTAL_CPUS - EXP_CPUS),&task->linux_task->cpus_allowed);
	task->cpu_assignment = index+(TOTAL_CPUS - EXP_CPUS);
   	set_children_cpu(task->linux_task, task->cpu_assignment);
	return;
}

/***
Just debug function for containers being mapped to specific CPUs
***/
void printChainInfo() {
        int i;
        s64 max = chainlength[0];
        for (i=0; i<number_of_heads; i++)
        {
	    printk(KERN_INFO "TimeKeeper: Print Chain Info: Length of chain %d is %lld\n", i, chainlength[i]);
            if (chainlength[i] > max)
                    max = chainlength[i];
        }
}

/***
Get the next task that is allowed to run this round, this means if the running_time > 0 (CBE specific)
***/
struct dilation_task_struct * getNextRunnableTask(struct dilation_task_struct * task) {
    if (task == NULL) {
		return NULL;
	}
	if (task->running_time > 0 && task->stopped != -1)
                return task;
        while (task->next != NULL) {
                task = task->next;
                if (task->running_time > 0 && task->stopped != -1)
                        return task;
    }
    /* if got through loop, this chain is done for this iteration, return NULL */
    return NULL;
}


/***
Calculate the amount by which the task should advance to reach the expected virtual time
 ***/
s64 calculate_change(struct dilation_task_struct* task, s64 virt_time, s64 expected_time) {
    s64 change;
	s32 rem;
	change = 0;
	unsigned long flags;

	spin_lock_irqsave(&task->linux_task->dialation_lock,flags);

	if (expected_time - virt_time < 0)
        {
            if (task->linux_task->dilation_factor > 0)
                    change = div_s64_rem( ((expected_time - virt_time)*-1)*task->linux_task->dilation_factor, PRECISION, &rem);
            else if (task->linux_task->dilation_factor < 0)
            {
                    change = div_s64_rem( ((expected_time - virt_time)*-1)*PRECISION, task->linux_task->dilation_factor*-1,&rem);
                    change += rem;
            }
            else if (task->linux_task->dilation_factor == 0)
                    change = (expected_time - virt_time)*-1;
			if (experiment_type == CS) {
        		change *= -1;
			}
        }
        else
        {
            if (task->linux_task->dilation_factor > 0)
                    change = div_s64_rem( (expected_time - virt_time)*task->linux_task->dilation_factor, PRECISION, &rem);
            else if (task->linux_task->dilation_factor < 0)
            {
                    change = div_s64_rem((expected_time - virt_time)*PRECISION, task->linux_task->dilation_factor*-1,&rem);
                    change += rem;
            }
            else if (task->linux_task->dilation_factor == 0)
            {
                    change = (expected_time - virt_time);
            }
			if (experiment_type == CBE) {
            	change *= -1;
			}
        }


	spin_unlock_irqrestore(&task->linux_task->dialation_lock,flags);
	
	return change;
}

/***
Find the difference between the tasks virtual time and the virtual time it SHOULD be at. Then it
will modify the tasks running_time, to allow it to run either more or less in the next round
***/
void calculate_virtual_time_difference(struct dilation_task_struct* task, s64 now, s64 expected_time)
{
	s64 change;
	ktime_t ktime;
	s64 virt_time;


	virt_time = get_virtual_time(task, now);
	change = 0;
	change = calculate_change(task, virt_time, expected_time);

	if (experiment_type == CS) {
	        if (change < 0)
        	{
	        	ktime = ktime_set(0, 0);
	        }
        	else
	        {
        		ktime = ktime_set(0, change);
	        }
	}
	if (experiment_type == CBE) {
        	if (change > task->running_time)
        	{
                	ktime = ktime_set(0, 0);
        	}
        	else
        	{
                	ktime = ktime_set(0, task->running_time - change);
        	}
	}
	
    task->stopped = 0;
    task->running_time = ktime_to_ns(ktime);
    return;
}

/***
The function called by each synchronization thread (CBE specific). For every process it is in charge of
it will see how long it should run, then start running the process at the head of the chain.
***/
int calculate_sync_drift(void *data)
{
	int round = 0;
	int cpuID = *((int *)data);
	struct dilation_task_struct *task;
	s64 now;
	struct timeval ktv;
	ktime_t ktime;
	int run_cpu;

	//printk(KERN_INFO "TimeKeeper: Calculate Sync Drift: Value passed to worker thread: %d\n", cpuID);
	if(atomic_read(&wake_up_signal_sync_drift[cpuID]) != 1)
		atomic_set(&wake_up_signal_sync_drift[cpuID],0);

	set_current_state(TASK_INTERRUPTIBLE);
	/* if it is the very first round, don't try to do any work, just rest */
	if (round == 0)
		goto startWork;
	
	while (!kthread_should_stop())
	{
		
        if(experiment_stopped == STOPPING)
        	return 0;

		task = chainhead[cpuID];
	    do_gettimeofday(&ktv);

		/* for every task it is responsible for, determine how long it should run */
		while (task != NULL) {
    	    now = timeval_to_ns(&ktv);
			calculate_virtual_time_difference(task, now, actual_time);
			task = task->next;
		}

		/* find the first task to run, then run it */
        task = chainhead[cpuID];

		if (task == NULL) {

			if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: Calculate Sync Drift: No Tasks in chain %d \n", cpuID);
			atomic_inc(&running_done);
			atomic_inc(&start_count);
		}
		else {
			do{
		        task = getNextRunnableTask(task);
    	       	if (task != NULL)
    	       	{
    	       		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
						printk(KERN_INFO "TimeKeeper: Calculate Sync Drift: Called  UnFreeze Proc Recurse on CPU: %d\n", cpuID);
					unfreeze_proc_exp_recurse(task, actual_time);

					if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
						printk(KERN_INFO "TimeKeeper : Calculate Sync Drift: Finished Unfreeze Proc on CPU: %d\n", cpuID);
    	           	task = task->next;
				
               	}
               	else
               	{
               		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
                  		printk(KERN_INFO "TimeKeeper: Calculate Sync Drift: %d chain %d has nothing to run\n",round,cpuID);
					break;
               	}
			

			}while(task != NULL);
		}

		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Calculate Sync Drift: Thread done with on %d\n",cpuID);

		/* when the first task has started running, signal you are done working, and sleep */
		round++;
		set_current_state(TASK_INTERRUPTIBLE);
		atomic_dec(&worker_count);
		atomic_set(&wake_up_signal_sync_drift[cpuID],0);
		run_cpu = get_cpu();
		

		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: #### Calculate Sync Drift: Sending wake up from Sync drift Thread for lxc on %d on run cpu %d\n",cpuID,run_cpu);

		wake_up_interruptible(&wq);
		

	startWork:
		schedule();
		set_current_state(TASK_RUNNING);
		run_cpu = get_cpu();

		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: ~~~~ Calculate Sync Drift: I am woken up for lxc on %d. run cpu = %d\n",cpuID,run_cpu);
		
	}
	return 0;
}

/***
The main synchronization thread (For CBE mode). When all tasks in a round have completed, this will get
woken up, increment the experiment virtual time,  and then wake up every other synchronization thread 
to have it do work
***/
int catchup_func(void *data)
{
        int round_count;
        struct timeval ktv;
        int i;
        int redo_count;
        round_count = 0;
        set_current_state(TASK_INTERRUPTIBLE);

		while (!kthread_should_stop())
        {
            round_count++;
			redo_count = 0;
            redo:

            if (experiment_stopped == STOPPING)
            {

					printk(KERN_INFO "TimeKeeper: Catchup Func: Cleaning experiment via catchup task\n");
                    clean_exp();
					set_current_state(TASK_INTERRUPTIBLE);	
					schedule();
					continue;

            }

            actual_time += expected_increase;

			/* clean up any stopped containers, alter TDFs if necessary */
			clean_stopped_containers();
			if (dilation_change)
				change_containers_dilation();

            do_gettimeofday(&ktv);
			atomic_set(&start_count, 0);

			/* wait up each synchronization worker thread, then wait til they are all done */
			if (number_of_heads > 0) {

				if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE) {
					printk(KERN_INFO "TimeKeeper: Catchup Func: Round finished. Waking up worker threads to calculate next round time\n");
					printk(KERN_INFO "TimeKeeper: Catchup Func: Current FREEZE_QUANTUM : %d\n", FREEZE_QUANTUM);
				}

				atomic_set(&worker_count, number_of_heads);
			
				for (i=0; i<number_of_heads; i++) {
					curr_sync_task_finished_flag[i] = 1;
					atomic_set(&wake_up_signal_sync_drift[i],1);	
	
					/* chaintask refers to calculate_sync_drift thread */
					if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE) {			
						if(wake_up_process(chaintask[i]) == 1){ 
							printk(KERN_INFO "TimeKeeper: Catchup Func: Sync thread %d wake up\n",i);
						}
						else{
							printk(KERN_INFO "TimeKeeper: Catchup Func: Sync thread %d already running\n",i);
						}
					}
	
				
				}

				int run_cpu;
				run_cpu = get_cpu();

				if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
					printk(KERN_INFO "TimeKeeper: Catchup Func: Waiting for sync drift threads to finish on run_cpu %d\n",run_cpu);

				wait_event_interruptible(wq, atomic_read(&worker_count) == 0);
				set_current_state(TASK_INTERRUPTIBLE);

				if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
					printk(KERN_INFO "TimeKeeper: Catchup Func: All sync drift thread finished\n");			
			}

			/* if there are no continers in the experiment, then stop the experiment */
			if (proc_num == 0 && experiment_stopped == RUNNING) {
				printk(KERN_INFO "TimeKeeper: Catchup Func: Cleaning experiment via catchup task because no tasks left\n");
                		clean_exp();	
				set_current_state(TASK_INTERRUPTIBLE);	
				schedule();
				continue;
	  		
			}
		    else if ((number_of_heads > 0 && atomic_read(&start_count) == number_of_heads) || (number_of_heads > 0 && atomic_read(&running_done) == number_of_heads))
		    { 	/* something bad happened, because not a single task was started, think due to page fault */
			    redo_count++;
			    atomic_set(&running_done, 0);
			    printk(KERN_INFO "TimeKeeper: Catchup Func: %d Redo computations %d Proc_num: %d Exp stopped: %d\n",round_count, redo_count, proc_num, experiment_stopped);
			    goto redo;
		    }

			end:
                	set_current_state(TASK_INTERRUPTIBLE);	
		
			if(experiment_stopped == NOTRUNNING){
				set_current_state(TASK_INTERRUPTIBLE);	
				schedule();			
			}
			printk(KERN_INFO "TimeKeeper : Catchup Func: Resumed\n");

        }
        return 0;
}

/***
If a LXC had its TDF changed during an experiment, modify the experiment accordingly (ie, make it the
new leader, and so forth)
***/
void change_containers_dilation() {
        struct list_head *pos;
        struct list_head *n;
        struct dilation_task_struct* task;
        struct dilation_task_struct* possible_leader;
        int new_highest;
		unsigned long flags;

        new_highest = -99999;
        possible_leader = NULL;

        list_for_each_safe(pos, n, &exp_list)
        {
            task = list_entry(pos, struct dilation_task_struct, list);

			/* its stopped, so skip it */
            if (task->stopped  == -1) 
            {
				continue;
            }
			if (task->newDilation != -1) {

				/* change its dilation */
				dilate_proc_recurse_exp(task->linux_task->pid, task->newDilation); 
				task->newDilation = -1; 
			
				/* update its runtime */
				calcTaskRuntime(task);  
			}

			spin_lock_irqsave(&task->linux_task->dialation_lock,flags);
            if (task->linux_task->dilation_factor > new_highest)
            {
                new_highest = task->linux_task->dilation_factor;
                possible_leader = task;
            }
			spin_unlock_irqrestore(&task->linux_task->dialation_lock,flags);

        }

		if (new_highest > exp_highest_dilation || new_highest < exp_highest_dilation)
	    {
	        /* If we have a new highest dilation, or if the leader container finished - save the new leader  */
            exp_highest_dilation = new_highest;
            leader_task = possible_leader;
            if (leader_task != NULL)
            {
                calcExpectedIncrease();
		        printk(KERN_INFO "TimeKeeper: Change Containers Dilation: New highest dilation is: %d new expected_increase: %lld\n", exp_highest_dilation, expected_increase);
            }
            /* update running time for each container because we have a new leader */
            list_for_each_safe(pos, n, &exp_list)
            {
                task = list_entry(pos, struct dilation_task_struct, list);
                calcTaskRuntime(task);
            }
        }

		/* reset global flag */
		dilation_change = 0; 
}

/***
If a container stops in the middle of an experiment, clean it up
***/
void clean_stopped_containers() {
    struct list_head *pos;
    struct list_head *n;
    struct dilation_task_struct* task;
    struct dilation_task_struct* next_task;
    struct dilation_task_struct* prev_task;
    struct dilation_task_struct* possible_leader;
    int new_highest;
    int did_leader_finish;
	unsigned long flags;

    did_leader_finish = 0;
    new_highest = -99999;
    possible_leader = NULL;

	/* for every container in the experiment, see if it has finished execution, if yes, clean it up.
	also check the possibility of needing to determine a new leader */
    list_for_each_safe(pos, n, &exp_list)
    {
		task = list_entry(pos, struct dilation_task_struct, list);
        if (task->stopped  == -1) //if stopped = -1, the process is no longer running, so free the structure
        {

			printk(KERN_INFO "TimeKeeper : Clean Stopped Containers: Detected stopped task\n");
          	if (leader_task == task)
			{ 
				/* the leader task is done, we NEED a new leader */
           		did_leader_finish = 1;
           	}
           	if (&task->timer != NULL)
           	{
           		hrtimer_cancel( &task->timer );
           	}

			prev_task = task->prev;
			next_task = task->next;

			/* handle head/tail logic */
			if (prev_task == NULL && next_task == NULL) {
				chainhead[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] = NULL;
				printk(KERN_INFO "TimeKeeper: Clean Stopped Containers: Stopping only head task for cPUID %d\n", task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS));
			}
			else if (prev_task == NULL) { 
				/* the stopped task was the head */
				chainhead[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] = next_task;
				next_task->prev = NULL;
			}
			else if (next_task == NULL) { //the stopped task was the tail
				prev_task->next = NULL;
			}
			else { 
				/* somewhere in the middle */
				prev_task->next = next_task;
				next_task->prev = prev_task;
			}

			chainlength[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] -= task->running_time;

			proc_num--;
           	printk(KERN_INFO "TimeKeeper: Clean Stopped Containers: Process %d is stopped!\n", task->linux_task->pid);
			list_del(pos);
           	kfree(task);
			continue;
        }

		spin_lock_irqsave(&task->linux_task->dialation_lock,flags);
        if (task->linux_task->dilation_factor > new_highest)
        {
        	new_highest = task->linux_task->dilation_factor;
                possible_leader = task;
        }
		spin_unlock_irqrestore(&task->linux_task->dialation_lock,flags);

	}
    if (new_highest > exp_highest_dilation || did_leader_finish == 1)
    {   
		/* If we have a new highest dilation, or if the leader container finished - save the new leader */
    	exp_highest_dilation = new_highest;
        leader_task = possible_leader;
        if (leader_task != NULL)
        {
           	calcExpectedIncrease();
			printk(KERN_INFO "TimeKeeper: Clean Stopped Containers: New highest dilation is: %d new expected_increase: %lld\n", exp_highest_dilation, expected_increase);
        }

        /* update running time for each container because we have a new leader */
        list_for_each_safe(pos, n, &exp_list)
        {
           	task = list_entry(pos, struct dilation_task_struct, list);
            calcTaskRuntime(task);
        }
    }
	stopped_change = 0;
    return;
}

/***
What gets called when a containers hrtimer interrupt occurs: the task is frozen, then it determines the next container that
should be ran within that round.
***/
enum hrtimer_restart exp_hrtimer_callback( struct hrtimer *timer )
{
	int dil;
	struct dilation_task_struct *task;
	struct dilation_task_struct * callingtask;
	struct timeval tv;
	int startJob;
	ktime_t ktime;
	task = container_of(timer, struct dilation_task_struct, timer);
	dil = task->linux_task->dilation_factor;
	callingtask = task;
	int CPUID = callingtask->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);

	if (catchup_task == NULL) {

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Hrtimer Callback: Proc called but catchup_task is null\n");
		return HRTIMER_NORESTART;
	}

	/* if the process is done, dont bother freezing it, just set flag so it gets cleaned in sync phase */
	if (callingtask->stopped == -1) {
		stopped_change = 1;
		curr_process_finished_flag[CPUID] = 1;
		atomic_set(&wake_up_signal[CPUID], 1);
		wake_up(&per_cpu_wait_queue[CPUID]);
		wake_up_process(chaintask[CPUID]);

	}
	else { 
		/* its not done, so freeze */
		task->stopped = 1;
		curr_process_finished_flag[CPUID] = 1;
		atomic_set(&wake_up_signal[CPUID], 1);
		wake_up(&per_cpu_wait_queue[CPUID]);
		wake_up_process(chaintask[CPUID]);		
	
	}

    return HRTIMER_NORESTART;
}
/***
Set's the freeze times of the process and all it's children to the specified argument 
***/
void set_all_freeze_times_recurse(struct task_struct * aTask, s64 freeze_time,s64 last_ppp, int max_no_recursions){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	unsigned long flags;
	s32 rem;

	if(max_no_recursions >= 100)
		return;

        if (aTask == NULL) {

        	if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
            	printk(KERN_INFO "TimeKeeper: Set all freeze times: Task does not exist\n");
            return;
        }
        if (aTask->pid == 0) {

        	if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: Set all freeze times: pid 0 error\n");
            return;
        }

	me = aTask;
	t = me;
	do {

		spin_lock_irqsave(&t->dialation_lock,flags);
		if (t->freeze_time == 0 && t->wakeup_time ==  0)
       	{
	        t->freeze_time = freeze_time;
			if(last_ppp != -1){
				t->past_physical_time = t->past_physical_time + last_ppp;
				if(aTask->dilation_factor > 0)
					t->past_virtual_time = t->past_virtual_time + div_s64_rem(last_ppp*PRECISION,aTask->dilation_factor,&rem);
				else
					t->past_virtual_time = t->past_virtual_time + last_ppp;	
			}
       	}
		else {

			/* doesn't enter this as freeze time being set to -1 is disabled */
			if(t->freeze_time == -1 && t->wakeup_time == 0){ 
				t->freeze_time = freeze_time;
				if(last_ppp != -1)
					t->past_physical_time = t->past_physical_time + last_ppp;

			}
			else{

				/* freeze_time > 0 and wakeup_time == 0 */
				if(t->wakeup_time == 0){ 
					if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
        	       		printk(KERN_INFO "TimeKeeper: Set all freeze times: Warning. Thread already frozen. pid: %d dilation %d\n", t->pid, t->dilation_factor);
					
					if(aTask->dilation_factor > 0) {
						t->past_virtual_time = t->past_virtual_time + div_s64_rem((freeze_time - t->freeze_time)*PRECISION,aTask->dilation_factor,&rem);
					}
					else {
						t->past_virtual_time = t->past_virtual_time + freeze_time - t->freeze_time;
					}
				}
			}
		}
		spin_unlock_irqrestore(&t->dialation_lock,flags);

	} while_each_thread(me, t);

	int i = 0;
    list_for_each(list, &aTask->children)
    {
		if(i > 1000)
			return;
        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
                continue;
        }

        set_all_freeze_times_recurse(taskRecurse, freeze_time,last_ppp,max_no_recursions + 1);
		i++;

    }
}


/***
Set's the past physical times of the process and all it's children to the specified argument 
***/
void set_all_past_physical_times_recurse(struct task_struct * aTask, s64 time, int max_no_of_recursions){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	unsigned long flags;

	if(max_no_of_recursions >= 100)
		return;

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

	me = aTask;
	t = me;
	do {
		spin_lock_irqsave(&t->dialation_lock,flags);
		if (t->freeze_time > 0 && t->wakeup_time ==  0)
       	{
			t->past_physical_time = t->past_physical_time + (time - t->freeze_time);
			t->freeze_time = 0;
			
       	}
		else {

			/* freeze_time <= 0 and wakeup_time == 0 */
			if(t->wakeup_time == 0){ 
				if(t != aTask){

					/* force set this thread's past physical time to that of the main thread */
					t->past_physical_time = aTask->past_physical_time; 
				}
				else{
					printk(KERN_INFO "TimeKeeper: Set all past physical times: Warning. Process not frozen pid: %d dilation %d\n", t->pid, t->dilation_factor);
					
				}
			}
		}
		spin_unlock_irqrestore(&t->dialation_lock,flags);
		
	} while_each_thread(me, t);

	int i = 0;
    list_for_each(list, &aTask->children)
    {
		if(i > 1000)
			return;

        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
                continue;
        }

        set_all_past_physical_times_recurse(taskRecurse, time,max_no_of_recursions + 1);
		i++;

    }
}

/***
Searches an lxc for the process with given pid. returns success if found. Max no of recursiond is just for safety
***/

struct task_struct * search_lxc(struct task_struct * aTask, int pid, int max_no_of_recursions){


	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;

	if(max_no_of_recursions >= 100)
		return NULL;

	if (aTask == NULL) {
		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: search lxc: Task does not exist\n");

		return NULL;
	}

	if (aTask->pid == 0) {
		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: search lxc: pid 0 error\n");
			return NULL;
	}

	me = aTask;
	t = me;

	int i = 0;
	if(t->pid == pid)
		return t;

	do {
		if (t->pid == pid)
       	{
			return t;			
       	}
	} while_each_thread(me, t);

	list_for_each(list, &aTask->children)
	{
		if(i > 1000)
			return NULL;

		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
			continue;
		}
		t =  search_lxc(taskRecurse, pid, max_no_of_recursions + 1);
		if(t != NULL)
			return t;
		i++;
	}

	return NULL;
}


/***
Actually cleans up the experiment by freeing all memory associated with the every container
***/
void clean_exp() {
	struct list_head *pos;
	struct list_head *n;
	struct dilation_task_struct *task;
	int ret;
	int i;
	struct sched_param sp;
	struct timeline* curr;
	struct timeline* tmp;
	struct timeval now;
	s64 now_ns;
	unsigned long flags;

	printk(KERN_INFO "TimeKeeper: Clean Exp: Starting Experiment Cleanup\n");
	ret = 0;
	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);

	/* free any heap memory associated with each container, cancel corresponding timers */
    list_for_each_safe(pos, n, &exp_list)
    {
        task = list_entry(pos, struct dilation_task_struct, list);
		sp.sched_priority = 0;
		if (experiment_stopped != NOTRUNNING) {
			
			resume_all(task->linux_task,task);				
			if (experiment_type == CS || experiment_type == CBE){
	
				spin_lock_irqsave(&task->linux_task->dialation_lock,flags);
				task->linux_task->past_physical_time = task->linux_task->past_physical_time + (now_ns - task->linux_task->freeze_time);
				task->linux_task->freeze_time = 0;
				task->linux_task->virt_start_time = 0;
				spin_unlock_irqrestore(&task->linux_task->dialation_lock,flags);

				kill(task->linux_task,SIGCONT,NULL);
				unfreeze_children(task->linux_task,now_ns,task->expected_time,task);

			}

            if (task->stopped != -1 && ret != -1) {
                sp.sched_priority = 0;
                if (sched_setscheduler(task->linux_task, SCHED_NORMAL, &sp) == -1 )
                        printk(KERN_INFO "TimeKeeper: Clean Exp: Error setting policy: %d pid: %d\n", SCHED_NORMAL, task->linux_task->pid);
                            
         		set_children_policy(task->linux_task, SCHED_NORMAL, 0);
        		cpumask_setall(&task->linux_task->cpus_allowed);
	
				/* -1 to fill cpu mask so that they can be scheduled in any cpu */
				set_children_cpu(task->linux_task, -1); 
			}
        }
		list_del(pos);
        if (&task->timer != NULL)
        {
                ret = hrtimer_cancel( &task->timer );
                if (ret) printk(KERN_INFO "TimeKeeper: Clean Exp: The timer was still in use...\n");
        }
		clean_up_schedule_list(task);
		kfree(task);
	}

    printk(KERN_INFO "TimeKeeper: Clean Exp: Linked list deleted\n");
    for (i=0; i<number_of_heads; i++) //clean up cpu specific chains
    {
		chainhead[i] = NULL;
		chainlength[i] = 0;
		if (experiment_stopped != NOTRUNNING) {
			printk(KERN_INFO "TimeKeeper: Clean Exp: Stopping chaintask %d\n", i);
			if (chaintask[i] != NULL && kthread_stop(chaintask[i]) )
				{
		        		printk(KERN_INFO "TimeKeeper: Clean Exp: Stopping worker %d error\n", i);
				}
			printk(KERN_INFO "TimeKeeper: Clean Exp: Stopped chaintask %d\n", i);
		}

		/* clean up timeline structs */
   		if (experiment_type == CS) {

			curr = timelineHead[i];
			timelineHead[i] = NULL;
			tmp = curr;
			while (curr != NULL) {
				tmp = curr;

				/* move to next timeline */
				curr = curr->next; 
				atomic_set(&tmp->pthread_done,1);
				atomic_set(&tmp->progress_thread_done,1);
				atomic_set(&tmp->stop_thread,2);
				wake_up_interruptible_sync(&tmp->pthread_queue);	
				wake_up_interruptible_sync(&tmp->progress_thread_queue);				
				printk(KERN_INFO "TimeKeeper : Clean Exp: Stopped all kernel threads for timeline : %d\n",tmp->number);
			}
		}
	}

	#ifdef __x86_64
		kill(loop_task, SIGSTOP, NULL);
	#endif

	experiment_stopped = NOTRUNNING;
	if(experiment_type != NOTSET){

		orig_cr0 = read_cr0();
		write_cr0(orig_cr0 & ~0x00010000);
		sys_call_table[__NR_poll] = (unsigned long *)ref_sys_poll;	
		sys_call_table[__NR_select_dialated] = (unsigned long *)ref_sys_select;	
		write_cr0(orig_cr0);
	}

	experiment_type = NOTSET;
	proc_num = 0;

	/* reset highest_dilation */
	exp_highest_dilation = -100000000; 
	leader_task = NULL;
	atomic_set(&running_done, 0);
	number_of_heads = 0;
	stopped_change = 0;

	printk(KERN_INFO "TimeKeeper: Clean Exp: Exited Clean Experiment");

	

}

/***
Sets the experiment_stopped flag, signalling catchup_task (sync function) to stop at the end of the current round and clean up
***/
void set_clean_exp() {

		/* assuming stopExperiment will not be called if still waiting for a S3F progress to return */
		if (experiment_stopped == NOTRUNNING || experiment_type == CS || experiment_stopped == FROZEN) {

		    /* sync experiment was never started, so just clean the list */
			printk(KERN_INFO "TimeKeeper: Set Clean Exp: Clean up immediately..\n");
			experiment_stopped = STOPPING;
		    clean_exp();
		}
		else if (experiment_stopped == RUNNING) {

			/* the experiment is running, so set the flag */
			printk(KERN_INFO "TimeKeeper: Set Clean Exp: Wait for catchup task to run before cleanup\n");
			experiment_stopped = STOPPING;
		}

        return;
}

/***
Set the time dilation variables to be consistent with all children
***/
void set_children_time(struct task_struct *aTask, s64 time) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct task_struct *me;
    struct task_struct *t;
	unsigned long flags;

	if (aTask == NULL) {
        printk(KERN_INFO "TimeKeeper: Set Children Time: Task does not exist\n");
        return;
    }
    if (aTask->pid == 0) {
                return;
    }
	me = aTask;
	t = me;

	/* set it for all threads */
	do {
		spin_lock_irqsave(&t->dialation_lock,flags);
		if (t->pid != aTask->pid) {
           		t->virt_start_time = time;
           		t->freeze_time = time;
           		t->past_physical_time = 0;
           		t->past_virtual_time = 0;
			if(experiment_stopped != RUNNING)
     	       	t->wakeup_time = 0;
		}
		spin_unlock_irqrestore(&t->dialation_lock,flags);
		
	} while_each_thread(me, t);


	list_for_each(list, &aTask->children)
	{
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
		        return;
		}
		spin_lock_irqsave(&taskRecurse->dialation_lock,flags);
		taskRecurse->virt_start_time = time;
		taskRecurse->freeze_time = time;
		taskRecurse->past_physical_time = 0;
		taskRecurse->past_virtual_time = 0;
		if(experiment_stopped != RUNNING)
		    taskRecurse->wakeup_time = 0;
		spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);
		set_children_time(taskRecurse, time);
	}
}

/***
Set the time dilation variables to be consistent with all children
***/
void set_children_policy(struct task_struct *aTask, int policy, int priority) {
        struct list_head *list;
        struct task_struct *taskRecurse;
        struct sched_param sp;
        struct task_struct *me;
        struct task_struct *t;

		if (aTask == NULL) {
		    printk(KERN_INFO "TimeKeeper: Set Children Policy: Task does not exist\n");
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
		   		sp.sched_priority = priority; 
				if (sched_setscheduler(t, policy, &sp) == -1 )
		    	    printk(KERN_INFO "TimeKeeper: Set Children Policy: Error setting thread policy: %d pid: %d\n",policy,t->pid);
			}

		} while_each_thread(me, t);

		list_for_each(list, &aTask->children)
		{
	        taskRecurse = list_entry(list, struct task_struct, sibling);
	        if (taskRecurse->pid == 0) {
	                return;
	        }

			/* set children scheduling policy */
		    sp.sched_priority = priority; 
			if (sched_setscheduler(taskRecurse, policy, &sp) == -1 )
		        printk(KERN_INFO "TimeKeeper: Set Children Policy: Error setting policy: %d pid: %d\n",policy,taskRecurse->pid);
		    set_children_policy(taskRecurse, policy, priority);
		}
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
		printk(KERN_INFO "TimeKeeper: Set Children CPU: Task does not exist\n");
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
Freezes all children associated with a container
***/
int freeze_children(struct task_struct *aTask, s64 time) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct dilation_task_struct *dilTask;
    struct task_struct *me;
    struct task_struct *t;
	unsigned long flags;

    if (aTask == NULL) {
    	if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
        	printk(KERN_INFO "TimeKeeper: Freeze Children: Task does not exist\n");
        return 0;
    }
    if (aTask->pid == 0) {

    	if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Freeze Children: PID equals 0\n");
        return 0;
    }

	me = aTask;
	t = me;
	do {
		spin_lock_irqsave(&t->dialation_lock,flags);
		if (t->pid != aTask->pid) {
			if (t->wakeup_time > 0 ) {

				/* task already has a wakeup_time set, so its already frozen, dont need to do anything */
				t->freeze_time = time;
	
				/* to stop any threads */
				kill(t, SIGSTOP, NULL); 
			}
  			else if (t->freeze_time == 0) 
           		{
					/* if task is not frozen yet */
   	        		t->freeze_time = time;
           			kill(t, SIGSTOP, NULL);
           		}
			else {

				/* to stop any threads */
				kill(t, SIGSTOP, NULL); 
				if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
	               		printk(KERN_INFO "TimeKeeper: Freeze Children: Thread already frozen %d wakeuptime %lld\n", t->pid, t->wakeup_time);
			}
		}
		spin_unlock_irqrestore(&t->dialation_lock,flags);

	} while_each_thread(me, t);

    list_for_each(list, &aTask->children)
    {
            taskRecurse = list_entry(list, struct task_struct, sibling);
            if(taskRecurse == aTask || taskRecurse->pid == aTask->pid)
            	return 0;
            if (taskRecurse->pid == 0) {
                    continue;
            }
			dilTask = container_of(&taskRecurse, struct dilation_task_struct, linux_task);

			spin_lock_irqsave(&taskRecurse->dialation_lock,flags);
			if (taskRecurse->wakeup_time > 0 ) {

				/* task already has a wakeup_time set, so its already frozen, dont need to do anything */
				taskRecurse->freeze_time = time; 
				spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);
				kill(taskRecurse, SIGSTOP, dilTask);
			}
			else if (taskRecurse->freeze_time == 0) 
			{
						/* if task is not frozen yet */
						taskRecurse->freeze_time = time;
						spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);
				    	kill(taskRecurse, SIGSTOP, dilTask);
			}
			else {
				spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

				/* just in case - to stop all threads */
				kill(taskRecurse, SIGSTOP, dilTask); 
		
				if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
					printk(KERN_INFO "TimeKeeper: Freeze Children: Process already frozen %d wakeuptime %lld\n", taskRecurse->pid, taskRecurse->wakeup_time);
				return -1;
			}
		


   if (freeze_children(taskRecurse, time) == -1)
		return 0;
    }
	return 0;
}

/***
Freezes the container, then calls freeze_children to freeze all of the children
***/
int freeze_proc_exp_recurse(struct dilation_task_struct *aTask) {
	struct timeval ktv;
	s64 now;
	unsigned long flags;


    if (aTask->linux_task->freeze_time > 0)
    {
    	if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
            printk(KERN_INFO "TimeKeeper: Freeze Proc Exp Recurse: Process already frozen %d\n", aTask->linux_task->pid);
        return -1;
    }
	do_gettimeofday(&ktv);
	now = (timeval_to_ns(&ktv));

	spin_lock_irqsave(&aTask->linux_task->dialation_lock,flags);
    if(aTask->linux_task->freeze_time == 0)  
		aTask->linux_task->freeze_time = now;
	spin_unlock_irqrestore(&aTask->linux_task->dialation_lock,flags);

	kill(aTask->linux_task, SIGSTOP, aTask);


    freeze_children(aTask->linux_task, now);
    return 0;
}


/***
Unfreezes all children associated with a container
***/
int unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time,struct dilation_task_struct * lxc) {
	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
	unsigned long flags;

	if (aTask == NULL) {

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Unfreeze Children: Task does not exist\n");
		return 0;
	}
	if (aTask->pid == 0) {

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: Unfreeze Children: Pid is 0 in unfreeze\n");
		return 0;
	}

	me = aTask;
	t = me;
	do {
		spin_lock_irqsave(&t->dialation_lock,flags);

		if(experiment_stopped == STOPPING){
			t->virt_start_time = 0;
		}
		
		if (t->pid == aTask->pid) {
			spin_unlock_irqrestore(&t->dialation_lock,flags);
		}
		else {
			if (t->freeze_time > 0)
			{
				t->past_physical_time = t->past_physical_time + (time - t->freeze_time);
				t->freeze_time = 0;
				kill(t, SIGCONT, NULL);
			}
			else {

				/* just in case - to continue all threads */
				kill(t, SIGCONT, NULL); 
				if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
					printk(KERN_INFO "TimeKeeper: Unfreeze Children: Thread not Frozen. Pid: %d Dilation %d\n", t->pid, t->dilation_factor);
			}
			
			task_poll_helper = hmap_get(&poll_process_lookup,&t->pid);
			task_select_helper = hmap_get(&select_process_lookup,&t->pid);
			task_sleep_helper = hmap_get(&sleep_process_lookup,&t->pid);

			
			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){
				
				spin_unlock_irqrestore(&t->dialation_lock,flags);
				kill(t, SIGCONT, dilTask);

            }
            else {
				
				if(task_poll_helper != NULL){				
					spin_unlock_irqrestore(&t->dialation_lock,flags);

					task_poll_helper->done = 1;
					wake_up(&task_poll_helper->w_queue);
					kill(t, SIGCONT, NULL);

				}
				else if(task_select_helper != NULL){					
					spin_unlock_irqrestore(&t->dialation_lock,flags);

					task_select_helper->done = 1;
					wake_up(&task_select_helper->w_queue);
					kill(t, SIGCONT, NULL);
				}
				else if( task_sleep_helper != NULL) {				
					spin_unlock_irqrestore(&t->dialation_lock,flags);

					task_sleep_helper->done = 1;
					wake_up(&task_sleep_helper->w_queue);
					kill(t, SIGCONT, NULL);

				}
				else
					spin_unlock_irqrestore(&t->dialation_lock,flags);
                
 			}


		}
		
		

	} while_each_thread(me, t);

    list_for_each(list, &aTask->children)
    {
        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
            continue;
        }
		dilTask = container_of(&taskRecurse, struct dilation_task_struct, linux_task);
		

		if(experiment_stopped == STOPPING)
			taskRecurse->virt_start_time = 0;

		spin_lock_irqsave(&taskRecurse->dialation_lock,flags);
		if (taskRecurse->wakeup_time != 0 && expected_time > taskRecurse->wakeup_time) {
			
			if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: Unfreeze Children: PID : %d Time to wake up: %lld actual time: %lld\n", taskRecurse->pid, taskRecurse->wakeup_time, expected_time);
			taskRecurse->virt_start_time = aTask->virt_start_time;
            taskRecurse->freeze_time = 0;
			taskRecurse->past_physical_time = aTask->past_physical_time;
			taskRecurse->past_virtual_time = aTask->past_virtual_time;
			taskRecurse->wakeup_time = 0;
			spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

			task_sleep_helper = hmap_get(&sleep_process_lookup,&taskRecurse->pid);
			if(task_sleep_helper != NULL) {
				task_sleep_helper->done = 1;
				wake_up(&task_sleep_helper->w_queue);
			}
			
			/* just in case - to continue all threads */
			kill(taskRecurse, SIGCONT, dilTask); 
			
		}
		else if (taskRecurse->wakeup_time != 0 && expected_time < taskRecurse->wakeup_time) {
			taskRecurse->freeze_time = 0; 
			spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);
			
		}
		else if (taskRecurse->freeze_time > 0)
		{
			task_poll_helper = hmap_get(&poll_process_lookup,&taskRecurse->pid);
			task_select_helper = hmap_get(&select_process_lookup,&taskRecurse->pid);
			task_sleep_helper = hmap_get(&sleep_process_lookup,&taskRecurse->pid);

			taskRecurse->past_physical_time = taskRecurse->past_physical_time + (time - taskRecurse->freeze_time);
			taskRecurse->freeze_time = 0;
			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){
				
				spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);
				kill(taskRecurse, SIGCONT, dilTask);

            }
            else {
				
				if(task_poll_helper != NULL){				
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

					task_poll_helper->done = 1;
					wake_up(&task_poll_helper->w_queue);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else if(task_select_helper != NULL){					
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

					task_select_helper->done = 1;
					wake_up(&task_select_helper->w_queue);
					kill(taskRecurse, SIGCONT, NULL);
				}
				else if( task_sleep_helper != NULL) {				
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

					task_sleep_helper->done = 1;
					wake_up(&task_sleep_helper->w_queue);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);	
                
 			}
                
        }
		else {
			task_poll_helper = hmap_get(&poll_process_lookup,&taskRecurse->pid);
			task_select_helper = hmap_get(&select_process_lookup,&taskRecurse->pid);
			task_sleep_helper = hmap_get(&sleep_process_lookup,&taskRecurse->pid);

			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){

				if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
        			printk(KERN_INFO "TimeKeeper: Unfreeze Children: Process not frozen. Pid: %d Dilation %d\n", taskRecurse->pid, taskRecurse->dilation_factor);
				spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);
				kill(taskRecurse, SIGCONT, dilTask);		
			}
			else {

				if(task_poll_helper != NULL){				
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

					task_poll_helper->done = 1;
					wake_up(&task_poll_helper->w_queue);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else if(task_select_helper != NULL){					
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

					task_select_helper->done = 1;
					wake_up(&task_select_helper->w_queue);
					kill(taskRecurse, SIGCONT, NULL);
				}
				else if( task_sleep_helper != NULL) {				
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

					task_sleep_helper->done = 1;
					wake_up(&task_sleep_helper->w_queue);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else
					spin_unlock_irqrestore(&taskRecurse->dialation_lock,flags);

			}
			
			/* no need to unfreeze its children	*/	
            return -1;					
		}
        if (unfreeze_children(taskRecurse, time, expected_time,lxc) == -1)
			return 0;
        }
	return 0;
}



/***
Resumes all children associated with a container. Called on cleanup
***/
int resume_all(struct task_struct *aTask,struct dilation_task_struct * lxc) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct dilation_task_struct *dilTask;
    struct task_struct *me;
    struct task_struct *t;
    struct poll_helper_struct * helper;
    struct select_helper_struct * sel_helper;
    struct sleep_helper_struct * sleep_helper;
	unsigned long flags;

	if (aTask == NULL) {
		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Resume All: Task does not exist\n");
		return 0;
	}

	if (aTask->pid == 0) {
		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper: Resume All: PID equals 0\n");
		return 0;
	}

	me = aTask;
	t = me;
	do {

		spin_lock_irqsave(&t->dialation_lock,flags);
		helper = hmap_get(&poll_process_lookup,&t->pid);
		sel_helper = hmap_get(&select_process_lookup,&t->pid);
		sleep_helper = hmap_get(&sleep_process_lookup,&t->pid);
		if(helper != NULL){
			t->wakeup_time = 0;
			t->freeze_time = 0;
			helper->err = FINISHED;
			helper->done = 1;
			spin_unlock_irqrestore(&t->dialation_lock,flags);
			wake_up(&helper->w_queue);

		}
		else if(sel_helper != NULL){

			t->wakeup_time = 0;
			t->freeze_time = 0;
			sel_helper->ret = FINISHED;
			sel_helper->done = 1;
			spin_unlock_irqrestore(&t->dialation_lock,flags);
			wake_up(&sel_helper->w_queue);
		}
		else if (sleep_helper != NULL) {
			t->wakeup_time = 0;
			t->freeze_time = 0;
			sleep_helper->done = 1;
			spin_unlock_irqrestore(&t->dialation_lock,flags);
			wake_up(&sleep_helper->w_queue);
		}
		else {
			spin_unlock_irqrestore(&t->dialation_lock,flags);
		}

	} while_each_thread(me, t);

	
    list_for_each(list, &aTask->children)
    {
		taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
            continue;
        }
	    resume_all(taskRecurse,lxc);
    }
	return 0;
}







/***
Get next task to run from the run queue of the lxc
***/
lxc_schedule_elem * get_next_valid_task(struct dilation_task_struct * lxc, s64 expected_time){

	struct pid *pid_struct;
	struct task_struct *task;
	int count = 0;
	struct poll_helper_struct * task_poll_helper;
	struct select_helper_struct * task_select_helper;
	int ret = 0;
	unsigned long flags;
 
	lxc_schedule_elem * head = schedule_list_get_head(lxc);
	if(head == NULL)
		return NULL;

	do{

		/* function to find the pid_struct */
		pid_struct = find_get_pid(head->pid); 	 

		/* find the task_struct. Also verifies if the task is still running */
		task = pid_task(pid_struct,PIDTYPE_PID); 
		task = search_lxc(lxc->linux_task,head->pid,0);

		if(task == NULL){	
			/* task is no longer running. remove from schedule queue */
			if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper : Get Next Valid Task: Task %d no longer running. Removing from schedule queue\n",head->pid);
			pop_schedule_list(lxc);
			head = schedule_list_get_head(lxc);
		}
		else{
			
			count ++; 
			head->curr_task= task;
			spin_lock_irqsave(&head->curr_task->dialation_lock,flags);
			head->curr_task->virt_start_time = lxc->linux_task->virt_start_time;

			/* This task cannot run now. need to look for another task */
			if(task->wakeup_time != 0 && task->wakeup_time > expected_time) 
			{

				spin_unlock_irqrestore(&head->curr_task->dialation_lock,flags);
				if(schedule_list_size(lxc) == 1 || count == schedule_list_size(lxc)) 
				{

					/* this is the only task or all tasks are simultaneously asleep. we have have problem ? */
					if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
						printk(KERN_INFO "TimeKeeper : Get Next Valid Task:  ERROR : All tasks simultaneously asleep\n");
					return NULL; // for now.
				}

				/* requeue the task to the tail */
				requeue_schedule_list(lxc); 		
				head = schedule_list_get_head(lxc);
			}		
			else{
				spin_unlock_irqrestore(&head->curr_task->dialation_lock,flags);
				return head;

			}
		}

	}while(head != NULL);

	/* Queue is empty. container stopped */
	return NULL; 	

	
}


/***
Add process and recursively its children to the run queue of the lxc
***/
void add_process_to_schedule_queue_recurse(struct dilation_task_struct * lxc, struct task_struct *aTask, s64 window_duration, s64 expected_inc){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	ktime_t ktime;
	struct timeval ktv;
	s64 now;
	unsigned long flags;

	if (aTask == NULL) {

		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
        	printk(KERN_INFO "TimeKeeper: Add Process To Schedule Queue: Task does not exist\n");
        return;
	}
	if (aTask->pid == 0) {

		if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper: Add Process To Schedule Queue: pid is 0 cannot be added to schedule queue\n");
		return;
	}


	spin_lock_irqsave(&aTask->dialation_lock,flags);
	aTask->dilation_factor = lxc->linux_task->dilation_factor;
	spin_unlock_irqrestore(&aTask->dialation_lock,flags);
	me = aTask;
	t = me;
	do {
		spin_lock_irqsave(&aTask->dialation_lock,flags);
		t->dilation_factor = aTask->dilation_factor;
		spin_unlock_irqrestore(&aTask->dialation_lock,flags);
		t->static_prio = aTask->static_prio;
		add_to_schedule_list(lxc,t,FREEZE_QUANTUM,exp_highest_dilation);
	} while_each_thread(me, t);

	/* If task already exists, schedule queue would not be modified */
	add_to_schedule_list(lxc,aTask,FREEZE_QUANTUM,exp_highest_dilation); 
	list_for_each(list, &aTask->children)
    {
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
				continue;
		}
		add_process_to_schedule_queue_recurse(lxc,taskRecurse,FREEZE_QUANTUM,exp_highest_dilation);
	}



}

/***
Refresh the run queue of the lxc at the start of every round to add new processes
***/
void refresh_lxc_schedule_queue(struct dilation_task_struct *aTask,s64 window_duration, s64 expected_inc){

	if(aTask != NULL){
		add_process_to_schedule_queue_recurse(aTask,aTask->linux_task,FREEZE_QUANTUM,exp_highest_dilation);
	}

}



/***
Unfreeze process at head of schedule queue of container, run it with possible switches for the run time. Returns the time left in this round.
***/ 
int run_schedule_queue_head_process(struct dilation_task_struct * lxc, lxc_schedule_elem * head, s64 remaining_run_time, s64 expected_time){

	struct list_head *list;
	struct task_struct * curr_task;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	ktime_t ktime;
	struct timeval ktv;
	s64 now;
	s64 timer_fire_time;
	s64 rem_time;
	s64 now_ns;
	struct poll_helper_struct * helper = NULL;
	struct select_helper_struct * select_helper = NULL;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;

	unsigned long flags;

	if(head->duration_left <= 0 || remaining_run_time <= 0){

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper : Run Schedule Queue Head Process: ERROR Cannot run task. duration left is 0");

		return remaining_run_time;

	}

	if(head->duration_left < remaining_run_time){
		timer_fire_time = head->duration_left;
		rem_time = remaining_run_time - head->duration_left;
	}
	else{
		timer_fire_time = remaining_run_time;
		rem_time = 0;
	}

	int CPUID = lxc->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);


	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);
	curr_task = head->curr_task;
	me = curr_task;
	t = me;	
	task_poll_helper = hmap_get(&poll_process_lookup,&t->pid);
	task_select_helper = hmap_get(&select_process_lookup,&t->pid);
	task_sleep_helper = hmap_get(&sleep_process_lookup, &t->pid);

	spin_lock_irqsave(&t->dialation_lock,flags);
	if(task_poll_helper != NULL){
		
		t->freeze_time = 0;
		spin_unlock_irqrestore(&t->dialation_lock,flags);

		task_poll_helper->done = 1;
		wake_up(&task_poll_helper->w_queue);
		kill(t, SIGCONT, NULL);

	}
	else if(task_select_helper != NULL){

		t->freeze_time = 0;
		spin_unlock_irqrestore(&t->dialation_lock,flags);

		task_select_helper->done = 1;
		wake_up(&task_select_helper->w_queue);
		kill(t, SIGCONT, NULL);
	}
	else if( task_sleep_helper != NULL) {
		t->freeze_time = 0;
		spin_unlock_irqrestore(&t->dialation_lock,flags);

		task_sleep_helper->done = 1;
		wake_up(&task_sleep_helper->w_queue);

		/* Sending a Continue signal here will wake all threads up. We dont want that */
		//kill(t, SIGCONT, NULL);

	}
	else if (t->freeze_time > 0 && t->wakeup_time == 0)
   	{	
		t->freeze_time = 0;
		spin_unlock_irqrestore(&t->dialation_lock,flags);
		if(kill(t, SIGCONT, NULL) < 0){
			return remaining_run_time;
		}
		
    	}
	else {
			if(t->freeze_time <= 0 && t->wakeup_time == 0){
				t->freeze_time = 0;
				spin_unlock_irqrestore(&t->dialation_lock,flags);
				if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
   		        	printk(KERN_INFO "TimeKeeper: Run Schedule Queue Head Process: Thread not frozen pid: %d dilation %d\n", t->pid, t->dilation_factor);
   		        
   		        	
				kill(t, SIGCONT, NULL);

			}
			else{

				if(t->wakeup_time > expected_time){
					
					if(helper == NULL && select_helper == NULL){

						spin_unlock_irqrestore(&t->dialation_lock,flags);
						if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
							printk(KERN_INFO "TimeKeeper: Run Schedule Queue Head Process: ERROR. Wakeup time still high\n");
						return remaining_run_time;			
					}			

					/* else it is a poll wakeup */
					if(lxc->last_run == NULL || lxc->last_run->curr_task == NULL){
						t->past_physical_time = 0;
						t->past_virtual_time = 0;
					
					}
					else{

							t->past_physical_time = lxc->last_run->curr_task->past_physical_time;
							t->past_virtual_time = lxc->last_run->curr_task->past_virtual_time;

					}

					if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
						printk(KERN_INFO "TimeKeeper : Run Schedule Queue Head Process: Time to Wake poll process %d\n",t->pid);

					t->freeze_time = 0;
					t->wakeup_time = 0;

					spin_unlock_irqrestore(&t->dialation_lock,flags);

				}
				else{

					/* update past physical time and virt start time with the last run process's past physical time and virt start time respectively */
					if(lxc->last_run == NULL || lxc->last_run->curr_task == NULL){
						t->past_physical_time = 0;
						t->past_virtual_time = 0;
					
					}
					else{

							t->past_physical_time = lxc->last_run->curr_task->past_physical_time;
							t->past_virtual_time = lxc->last_run->curr_task->past_virtual_time;

					}

					
					printk(KERN_INFO "TimeKeeper : Run Schedule Queue Head Process: Time to Wake up %d\n",t->pid);
					t->freeze_time = 0;
					t->wakeup_time = 0;

					spin_unlock_irqrestore(&t->dialation_lock,flags);


					struct sleep_helper_struct * sleep_helper = NULL;
					sleep_helper = hmap_get(&sleep_process_lookup,&t->pid);
					if(sleep_helper != NULL){
						sleep_helper->done = 1;
						wake_up(&sleep_helper->w_queue);
						
					}
					else{
						/* We dont want to wake all threads up if only one of them was sleeping */
						if(kill(t, SIGCONT, NULL) < 0){
							return remaining_run_time;				
						}
					}						

		
				}

			}
	}
		
	

	ktime = ktime_set( 0, timer_fire_time );
	int ret;
	set_current_state(TASK_INTERRUPTIBLE);
	if(experiment_type != CS){
		hrtimer_start(&lxc->timer,ktime,HRTIMER_MODE_REL);
		schedule();
	}
	else{
		hrtimer_start(&lxc->timer,ktime,HRTIMER_MODE_REL);
		wait_event_interruptible(lxc->tl->unfreeze_proc_queue,atomic_read(&lxc->tl->hrtimer_done) == 1);
		atomic_set(&lxc->tl->hrtimer_done,0);
	}
	set_current_state(TASK_RUNNING);
	curr_process_finished_flag[CPUID] = 0;

	head->duration_left = head->duration_left - timer_fire_time;
	if(head->duration_left <= 0){

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper : Run Schedule Queue Head Process: Resetting head to duration of %lld\n", head->share_factor);
		
		head->duration_left = head->share_factor;
		requeue_schedule_list(lxc);
	}
	
	me = curr_task;
	t = me;
	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);

	spin_lock_irqsave(&t->dialation_lock,flags);
	if (t->wakeup_time > 0 ) {
		spin_unlock_irqrestore(&t->dialation_lock,flags);
	
		/* send sigstop anyway to stop all threads */
		kill(t, SIGSTOP, NULL);
	}
	else if (t->freeze_time == 0) 
    {
			/* if task is not frozen yet */
			t->freeze_time = now_ns;
			spin_unlock_irqrestore(&t->dialation_lock,flags);

       		if(kill(t, SIGSTOP, NULL) < 0){
				return rem_time;
			}
		
    }
	else {
		spin_unlock_irqrestore(&t->dialation_lock,flags);

		/* sending sigstop anyway to stop all threads */
		kill(t, SIGSTOP, NULL); 
		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
       		printk(KERN_INFO "TimeKeeper: Run Schedule Queue Head Process : Thread already frozen %d wakeuptime %lld\n", t->pid, t->wakeup_time);
	}
	
	/* set the last run task */	
	lxc->last_run = head; 
	return rem_time;

	
}



/***
Unfreezes the container, then calls unfreeze_children to unfreeze all of the children
***/
int unfreeze_proc_exp_recurse(struct dilation_task_struct *aTask, s64 expected_time) {
	struct timeval now;
	s64 now_ns;
	s64 start_ns;
	struct hrtimer * alt_timer = &aTask->timer;
	int CPUID = aTask->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
	int i = 0;
	unsigned long flags;

	if (aTask->linux_task->freeze_time == 0)
	{
		printk(KERN_INFO "TimeKeeper: Unfreeze Proc Exp Recurse: Process not frozen pid: %d dilation %d in recurse\n", aTask->linux_task->pid, aTask->linux_task->dilation_factor);
		return -1;
	}
        
	atomic_set(&wake_up_signal_sync_drift[CPUID],0);

	/** Modified scheduling Logic **/
	if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
		printk(KERN_INFO "TimeKeeper : Unfreeze Proc Exp Recurse: Refreshing Schedule Queue on CPU : %d for lxc : %d\n",CPUID, aTask->linux_task->pid);
	
	/* for adding any new tasks that might have been spawned */
	refresh_lxc_schedule_queue(aTask,aTask->running_time,expected_time); 
	
	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);
	start_ns = now_ns;

	/* TODO */
	/* Needed to handle this case separately for some wierd bug which causes a crash */
	if(schedule_list_size(aTask) == 1){

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper : Unfreeze Proc Exp Recurse: Single process LXC on CPU %d\n",CPUID);

		spin_lock_irqsave(&aTask->linux_task->dialation_lock,flags);
        if(aTask->linux_task->freeze_time > 0) { 
			aTask->linux_task->past_physical_time = aTask->linux_task->past_physical_time + (now_ns - aTask->linux_task->freeze_time);
			aTask->linux_task->freeze_time = 0;
        }
  		spin_unlock_irqrestore(&aTask->linux_task->dialation_lock,flags);

		kill(aTask->linux_task,SIGCONT,NULL);
		unfreeze_children(aTask->linux_task,now_ns,expected_time,aTask);

		ktime_t ktime;
		ktime = ktime_set(0,aTask->running_time);
		set_current_state(TASK_INTERRUPTIBLE);
		if(experiment_type != CS){
			hrtimer_start(&aTask->timer,ktime,HRTIMER_MODE_REL);
			schedule();
		}
		else{
			hrtimer_start(&aTask->timer,ktime,HRTIMER_MODE_REL);
			wait_event_interruptible(aTask->tl->unfreeze_proc_queue,atomic_read(&aTask->tl->hrtimer_done) == 1);
			atomic_set(&aTask->tl->hrtimer_done,0);
		}
		set_current_state(TASK_RUNNING);

		if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
			printk(KERN_INFO "TimeKeeper : Unfreeze Proc Exp Recurse: Single process on CPU %d resumed\n",CPUID);
		
		freeze_proc_exp_recurse(aTask);
	

	}
	else{
		
		/* Set all past physical times */
		set_all_past_physical_times_recurse(aTask->linux_task, now_ns,0);
		lxc_schedule_elem * head;
		s64 rem_time = aTask->running_time;
		do{
	
	
			if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper : Unfreeze Proc Exp Recurse: Getting next valid task on CPU : %d for lxc : %d\n",CPUID, aTask->linux_task->pid);
			
			head = get_next_valid_task(aTask,expected_time);
			if(head == NULL){

				/* need to stop container here */
				if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
					printk(KERN_INFO "TimeKeeper : Unfreeze Proc Exp Recurse: ERROR. Need to stop container\n");
				return 0;

			}
			atomic_set(&wake_up_signal_sync_drift[CPUID],0);
			if(DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE)
				printk(KERN_INFO "TimeKeeper : Unfreeze Proc Exp Recurse: Running next valid task on CPU : %d for lxc : %d\n",CPUID, aTask->linux_task->pid);
			
			rem_time  = run_schedule_queue_head_process(aTask, head, rem_time, expected_time);
			i++;	
		}while(rem_time > 0);

		do_gettimeofday(&now);
		now_ns = timeval_to_ns(&now);

		/* Set all freeze times */
		set_all_freeze_times_recurse(aTask->linux_task, now_ns,aTask->running_time,0);
	
	}
        return 0;
}


/***
Given a pid and a new dilation, dilate it and all of it's children
***/
void dilate_proc_recurse_exp(int pid, int new_dilation) {
	struct task_struct *aTask;
    aTask = find_task_by_pid(pid);
    if (aTask != NULL) {
		change_dilation(pid, new_dilation);
		perform_on_children(aTask, change_dilation, new_dilation);
	}
}

/*** 
Gets the virtual time given a dilation_task_struct
***/
s64 get_virtual_time(struct dilation_task_struct* task, s64 now) {
	s64 virt_time;
	virt_time = get_virtual_time_task(task->linux_task, now);
    task->curr_virt_time = virt_time;
	return virt_time;
}



