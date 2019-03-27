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
void add_to_exp(int pid);
void addToChain(struct dilation_task_struct *task);
void assign_to_cpu(struct dilation_task_struct *task);
void printChainInfo(void);
void add_to_exp_proc(char *write_buffer);
void clean_exp(void);
void set_clean_exp(void);
void set_children_time(struct task_struct *aTask, s64 time);
int freeze_children(struct task_struct *aTask, s64 time);
int unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time);
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

extern struct task_struct *loop_task;
extern int TOTAL_CPUS;
extern struct timeline* timelineHead[EXP_CPUS];

extern void perform_on_children(struct task_struct *aTask, void(*action)(int,int), int val);
extern void change_dilation(int pid, int new_dilation);
extern s64 get_virtual_time_task(struct task_struct* task, s64 now);

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
        printk(KERN_INFO "TimeKeeper: Adding a pid: %d num: %d, number of heads %d\n", aTask->pid, proc_num, number_of_heads);
        list_node = (struct dilation_task_struct *)kmalloc(sizeof(struct dilation_task_struct), GFP_KERNEL);
        list_node->linux_task = aTask;
        list_node->stopped = 0;
        list_node->next = NULL;
        list_node->prev = NULL;
        list_node->wake_up_time = 0;
        list_node->newDilation = -1;
        list_node->increment = 0;
        list_node->cpu_assignment = -1;
        hrtimer_init( &list_node->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	return list_node;
}

//wake_up_process returns 1 if it was stopped and gets woken up, 0 if it is dead OR already running
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
		printk(KERN_INFO "TimeKeeper: Trying to add to wrong experiment type.. exiting\n");
	}
	else if (experiment_stopped == NOTRUNNING) {
        add_to_exp(pid);
	}
	else {
		printk(KERN_INFO "TimeKeeper: Trying to add a LXC to experiment that is already running\n");
	}
}

/***
Gets called by add_to_exp_proc(). Initiazes a containers timer, sets scheduling policy.
***/
void add_to_exp(int pid) {
        struct task_struct* aTask;
        struct dilation_task_struct* list_node;
        aTask = find_task_by_pid(pid);
        //maybe I should just skip this pid instead of completely dropping out?
        if (aTask == NULL)
        {
                printk(KERN_INFO "TimeKeeper: Pid %d is invalid, dropping out\n",pid);
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

	printk(KERN_INFO "TimeKeeper: ** Starting Experiment Synchronization **\n");

	if (proc_num == 0) {
		printk(KERN_INFO "TimeKeeper: Nothing added to experiment, dropping out\n");
		return;
	}

	if (experiment_stopped != NOTRUNNING) {
                printk(KERN_INFO "TimeKeeper: Trying to s3fStartExp when an experiment is already running!\n");
                return;
        }

	for (j = 0; j < number_of_heads; j++) {
        values[j] = j;
	}
        sp.sched_priority = 99;

	//Create the threads for parallel computing
	for (i = 0; i < number_of_heads; i++)
	{
		printk(KERN_INFO "TimeKeeper: Adding worker thread %d\n", i);
		chainhead[i] = NULL;
		chainlength[i] = 0;
		if (experiment_type == CBE)
			chaintask[i] = kthread_run(&calculate_sync_drift, &values[i], "worker");
	}

	//If in CBE mode, find the leader task (highest TDF)
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

        calcExpectedIncrease(); //calculate how far virtual time should advance every round

        do_gettimeofday(&now_timeval);
        now = timeval_to_ns(&now_timeval);
        actual_time = now;
	printk(KERN_INFO "TimeKeeper: Setting all tasks to be: %lld\n", actual_time);
        //for every container in the experiment, set the virtual_start_time (so it starts at the same time), calculate
	//how long each task should be allowed to run in each round, and freeze the container
	list_for_each_safe(pos, n, &exp_list)
        {
                list_node = list_entry(pos, struct dilation_task_struct, list);
                if (experiment_type == CBE)
			calcTaskRuntime(list_node);
                list_node->linux_task->virt_start_time = now; //consistent time
		if (experiment_type == CS) {
			list_node->expected_time = now;
			list_node->running_time = 0;
		}
                list_node->linux_task->past_physical_time = 0;
                list_node->linux_task->past_virtual_time = 0;
                list_node->linux_task->wakeup_time = 0;

		freeze_proc_exp_recurse(list_node); //freeze all children
                list_node->linux_task->freeze_time = now;

		//set priority and scheduling policy
                if (list_node->stopped == -1) {
                        printk(KERN_INFO "TimeKeeper: One of the LXCs no longer exist.. exiting experiment\n");
                        clean_exp();
                        return;
                }

        	if (sched_setscheduler(list_node->linux_task, SCHED_RR, &sp) == -1 )
                	printk(KERN_INFO "TimeKeeper: Error setting SCHED_RR %d\n",list_node->linux_task->pid);
                set_children_time(list_node->linux_task, now);
		set_children_policy(list_node->linux_task, SCHED_RR, 99);

		if (experiment_type == CS) {
			//printk(KERN_INFO "TimeKeeper: cpus allowed! : %d \n", list_node->linux_task->cpus_allowed);
     			bitmap_zero((&list_node->linux_task->cpus_allowed)->bits, 8);
		        cpumask_set_cpu(list_node->cpu_assignment, &list_node->linux_task->cpus_allowed);
			set_children_cpu(list_node->linux_task, list_node->cpu_assignment);
		}

		printk(KERN_INFO "TimeKeeper: Task running time: %lld\n", list_node->running_time);
	}

	//If in CBE mode, assign all tasks to a specfic CPU (this has already been done if in CS mode)
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

	//Set what mode experiment is in, depending on experiment_type (CBE or CS)
	if (experiment_type == CS)
		experiment_stopped = RUNNING;
	else
		experiment_stopped = FROZEN;

//if its 64-bit, start the busy loop task to fix the weird bug
#ifdef __x86_64
	kill(loop_task, SIGCONT, NULL);
#endif

}

/***
Specifies the start of the experiment (if in CBE mode)
***/
void core_sync_exp() {
        struct dilation_task_struct* list_node;
	int i;
        ktime_t ktime;

	if (experiment_type == CS) {
		printk(KERN_INFO "TimeKeeper: Trying to start wrong type of experiment.. exiting\n");
		return;
	}
	if (experiment_stopped != FROZEN) {
		printk(KERN_INFO "TimeKeeper: Experiment is not ready to commence, must run synchronizeAndFreeze\n");
		return;
	}
	//for every 'head' container, unfreeze it and set its timer to fire at some point in the future (based off running_time)
        for (i=0; i<number_of_heads; i++)
        {
                list_node = chainhead[i];
		printk(KERN_INFO "TimeKeeper: Unfreezing container with pid: %d\n",list_node->linux_task->pid);

             	unfreeze_proc_exp_recurse(list_node, actual_time);

                ktime = ktime_set( 0, list_node->running_time );
                hrtimer_start( &list_node->timer, ktime, HRTIMER_MODE_REL );
        }
	experiment_stopped = RUNNING;
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

        // temp_proc and temp_high are temporary dilations for the container and leader respectively.
        // this is done just to make sure the Math works (no divide by 0 errors if the TDF is 0, by making the temp TDF 1)
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
        // if the leaders' TDF and the containers TDF are the same, let it run for the full amount (do not need to scale)
                task->running_time = FREEZE_QUANTUM;
        }
        else if (exp_highest_dilation > 0 && dil > 0) {
        //if both the leaders' TDF and the containers TDF are > 0, scale running time
                task->running_time = (div_s64_rem(FREEZE_QUANTUM,exp_highest_dilation,&rem) + rem)*dil;
        }
        else if (exp_highest_dilation > 0 && dil == 0) {
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION,exp_highest_dilation,&rem) + rem);
	}
        else if (exp_highest_dilation > 0 && dil < 0) { //1st > 0, 2nd < 0
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION*PRECISION,exp_highest_dilation*temp_proc,&rem) + rem);

        }
        else if (exp_highest_dilation == 0 && dil < 0) { //1st == 0, 2nd < 0
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION,temp_proc,&rem) + rem);
        }
        else if (exp_highest_dilation < 0 && dil < 0){ //both < 0
                task->running_time = (div_s64_rem(FREEZE_QUANTUM,temp_proc,&rem) + rem)*temp_high;
        }
	else {
		printk("Should be fixed when highest dilation is updated\n");
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
	}
	else {
		while (walk->next != NULL)
		{
			walk = walk->next;
		}
		walk->next = task;
		task->prev = walk; //create doubly linked list
	}
	//set CPU mask
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
		printk(KERN_INFO "TimeKeeper: Length of chain %d is %lld\n", i, chainlength[i]);
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
        //if got through loop, this chain is done for this iteration, return NULL
        return NULL;
}

s64 calculate_change(struct dilation_task_struct* task, s64 virt_time, s64 expected_time) {
        s64 change;
	s32 rem;
	change = 0;
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
	return change;
}

/*
Find the difference between the tasks virtual time and the virtual time it SHOULD be at. Then it
will modify the tasks running_time, to allow it to run either more or less in the next round
*/
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
	//printk(KERN_INFO "TimeKeeper: %d change: %lld\n",task->linux_task->pid, change);
        task->stopped = 0;
        task->running_time = ktime_to_ns(ktime);
        return;
}

/*
The function called by each synchronization thread (CBE specific). For every process it is in charge of
it will see how long it should run, then start running the process at the head of the chain.
*/
int calculate_sync_drift(void *data)
{
	int round = 0;
	int cpuID = *((int *)data);
	struct dilation_task_struct *task;
	s64 now;
	struct timeval ktv;
	ktime_t ktime;

	printk("Value passed to worker thread: %d\n", cpuID);
	set_current_state(TASK_INTERRUPTIBLE);
	//if it is the very first round, don't try to do any work, just rest
	if (round == 0)
		goto startWork;
	while (!kthread_should_stop())
        {
		task = chainhead[cpuID];
	        do_gettimeofday(&ktv);
		//for every task it is responsible for, determine how long it should run
		while (task != NULL) {
       	                now = timeval_to_ns(&ktv);
			calculate_virtual_time_difference(task, now, actual_time);
			task = task->next;
		}

		//find the first task to run, then run it
                task = chainhead[cpuID];
		if (task == NULL) {
			printk(KERN_INFO "TimeKeeper: No Tasks in chain %d \n", cpuID);
			atomic_inc(&running_done);
			atomic_inc(&start_count);
		}
		else {
                        task = getNextRunnableTask(task);
                       	if (task != NULL)
                       	{
			//	printk(KERN_INFO "TimeKeeper: Thinkgs task is not null? %d\n", cpuID);
				unfreeze_proc_exp_recurse(task, actual_time);
                               	ktime = ktime_set( 0, task->running_time );
                               	hrtimer_start( &task->timer, ktime, HRTIMER_MODE_REL );
                       	}
                       	else
                       	{
                              	printk(KERN_INFO "TimeKeeper: %d chain %d has nothing to run\n",round,cpuID);
				atomic_inc(&running_done);
				atomic_inc(&start_count);
                       	}
		}

		//printk(KERN_INFO "TimeKeeper: Thread with done with %d\n",cpuID);
		//when the first task has started running, signal you are done working, and sleep
		round++;
		atomic_dec(&worker_count);
		wake_up_interruptible(&wq);
		startWork:
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}

/*
The main synchronization thread (For CBE mode). When all tasks in a round have completed, this will get
woken up, increment the experiment virtual time,  and then wake up every other synchronization thread 
to have it do work
*/
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
			printk(KERN_INFO "TimeKeeper: Cleaning experiment via catchup task\n");
                        clean_exp();
			goto end;
                }

                actual_time += expected_increase;

		//clean up any stopped containers, alter TDFs if necessary
		clean_stopped_containers();
		if (dilation_change)
			change_containers_dilation();

                do_gettimeofday(&ktv);
		atomic_set(&start_count, 0);

		//wait up each synchronization worker thread, then wait til they are all done
		if (number_of_heads > 0) {
			atomic_set(&worker_count, number_of_heads);
			for (i=0; i<number_of_heads; i++) {
				wake_up_process(chaintask[i]); // chaintask refers to calculate_sync_drift thread
			}
		wait_event_interruptible(wq, atomic_read(&worker_count) == 0);
		}

		//if there are no continers in the experiment, then stop the experiment
		if (proc_num == 0 && experiment_stopped == RUNNING) {
			printk(KERN_INFO "TimeKeeper: Cleaning experiment via catchup task because no tasks left\n");
                        clean_exp();
		}
                else if ((number_of_heads > 0 && atomic_read(&start_count) == number_of_heads) || (number_of_heads > 0 && atomic_read(&running_done) == number_of_heads))
                { //something bad happened, because not a single task was started, think due to page fault
                        redo_count++;
                        atomic_set(&running_done, 0);
                        printk(KERN_INFO "TimeKeeper: %d redo computations %d proc_num: %d exp stopped: %d\n",round_count, redo_count, proc_num, experiment_stopped);
                        goto redo;
                }

		end:
                set_current_state(TASK_INTERRUPTIBLE);
                schedule();
        }
        return 0;
}

/*
If a LXC had its TDF changed during an experiment, modify the experiment accordingly (ie, make it the
new leader, and so forth)
*/
void change_containers_dilation() {
        struct list_head *pos;
        struct list_head *n;
        struct dilation_task_struct* task;
        struct dilation_task_struct* possible_leader;
        int new_highest;

        new_highest = -99999;
        possible_leader = NULL;

        list_for_each_safe(pos, n, &exp_list)
        {
                task = list_entry(pos, struct dilation_task_struct, list);
                if (task->stopped  == -1) //its stopped, so skip it
                {
			continue;
                }
		if (task->newDilation != -1) {
			dilate_proc_recurse_exp(task->linux_task->pid, task->newDilation); //change its dilation
			task->newDilation = -1; //reset
			calcTaskRuntime(task); //update its runtime. // *** I don't think it is needed here.
		}
                if (task->linux_task->dilation_factor > new_highest)
                {
                        new_highest = task->linux_task->dilation_factor;
                        possible_leader = task;
                }
        }

	if (new_highest > exp_highest_dilation || new_highest < exp_highest_dilation)
        {        // If we have a new highest dilation, or if the leader container finished - save the new leader
                exp_highest_dilation = new_highest;
                leader_task = possible_leader;
                if (leader_task != NULL)
                {
                        calcExpectedIncrease();
		        printk(KERN_INFO "TimeKeeper: New highest dilation is: %d new expected_increase: %lld\n", exp_highest_dilation, expected_increase);
                }
                //update running time for each container because we have a new leader
                list_for_each_safe(pos, n, &exp_list)
                {
                        task = list_entry(pos, struct dilation_task_struct, list);
                        calcTaskRuntime(task);
                }
        }
	dilation_change = 0; //reset global flag
}

/*
If a container stops in the middle of an experiment, clean it up
*/
void clean_stopped_containers() {
        struct list_head *pos;
        struct list_head *n;
        struct dilation_task_struct* task;
        struct dilation_task_struct* next_task;
        struct dilation_task_struct* prev_task;
	struct dilation_task_struct* possible_leader;
        int new_highest;
        int did_leader_finish;

	did_leader_finish = 0;
        new_highest = -99999;
        possible_leader = NULL;
	//for every container in the experiment, see if it has finished execution, if yes, clean it up.
	// also check the possibility of needing to determine a new leader
        list_for_each_safe(pos, n, &exp_list)
        {
		task = list_entry(pos, struct dilation_task_struct, list);
                if (task->stopped  == -1) //if stopped = -1, the process is no longer running, so free the structure
                {
                	if (leader_task == task)
			{ //the leader task is done, we NEED a new leader)
                		did_leader_finish = 1;
                	}
                	if (&task->timer != NULL)
                	{
                		hrtimer_cancel( &task->timer );
                	}

			prev_task = task->prev;
			next_task = task->next;
			//handle head/tail logic
			if (prev_task == NULL && next_task == NULL) {
				chainhead[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] = NULL;
				printk(KERN_INFO "TimeKeeper: Stopping only head task for cPUID %d\n", task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS));
			}
			else if (prev_task == NULL) { //the stopped task was the head
				chainhead[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] = next_task;
				next_task->prev = NULL;
			}
			else if (next_task == NULL) { //the stopped task was the tail
				prev_task->next = NULL;
			}
			else { //somewhere in the middle
				prev_task->next = next_task;
				next_task->prev = prev_task;
			}

			chainlength[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] -= task->running_time;

			proc_num--;
                	printk(KERN_INFO "TimeKeeper: process %d is stopped!\n", task->linux_task->pid);
			list_del(pos);
                	kfree(task);
			continue;
                }
                if (task->linux_task->dilation_factor > new_highest)
                {
                	new_highest = task->linux_task->dilation_factor;
                        possible_leader = task;
                }
	}
        if (new_highest > exp_highest_dilation || did_leader_finish == 1)
        {        // If we have a new highest dilation, or if the leader container finished - save the new leader
        	exp_highest_dilation = new_highest;
                leader_task = possible_leader;
                if (leader_task != NULL)
                {
                	calcExpectedIncrease();
			printk(KERN_INFO "TimeKeeper: New highest dilation is: %d new expected_increase: %lld\n", exp_highest_dilation, expected_increase);
                }
                //update running time for each container because we have a new leader
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
        int startJob;
        ktime_t ktime;
        task = container_of(timer, struct dilation_task_struct, timer);
        dil = task->linux_task->dilation_factor;
        callingtask = task;
	if (catchup_task == NULL) {
	printk(KERN_INFO "TimeKeeper: hrtimer proc called but catchup_task is null\n");
		return HRTIMER_NORESTART;
	}

	//if the process is done, dont bother freezing it, just set flag so it gets cleaned in sync phase
	if (callingtask->stopped == -1) {
		stopped_change = 1;
        }
	else { //its not done, so freeze
		task->stopped = 1;
		if (freeze_proc_exp_recurse(task) == -1) {
				printk(KERN_INFO "TimeKeeper: Freezing return -1, exiting experiment.. why would it already be frozen?\n");
                        	return HRTIMER_NORESTART;
			}
	}

	startJob = 0;
	//find next task that has needs to run this round, then unfreeze it and start it's timer
        while (task->next != NULL)
        {
        	task = task->next;
                if (task->running_time > 0 && task->stopped != -1)
                {
                	unfreeze_proc_exp_recurse(task, actual_time);
                        ktime = ktime_set( 0, task->running_time );
                        hrtimer_start( &task->timer, ktime, HRTIMER_MODE_REL );
                        startJob = 1;
                        break;
        	}
        }

	//if there are no more tasks to run, then this chain of tasks are done, and ready to be sync'd
        if (startJob == 0)
        {
        	atomic_inc(&running_done);
                if (atomic_read(&running_done) == number_of_heads)
                {
	                atomic_set(&running_done, 0);
                        wake_up_process(catchup_task);
                }
        }
        return HRTIMER_NORESTART;
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
	printk(KERN_INFO "TimeKeeper: Starting Experiment Cleanup\n");
	ret = 0;
	//free any heap memory associated with each container, cancel corresponding timers
        list_for_each_safe(pos, n, &exp_list)
        {
                task = list_entry(pos, struct dilation_task_struct, list);
		sp.sched_priority = 0;
		if (experiment_stopped != NOTRUNNING) {
			if (experiment_type == CBE)
                        	ret = unfreeze_proc_exp_recurse(task, actual_time);
			else if (experiment_type == CS)
				ret = unfreeze_proc_exp_recurse(task, task->expected_time);
                        if (task->stopped != -1 && ret != -1) {
                                sp.sched_priority = 0;
                                if (sched_setscheduler(task->linux_task, SCHED_NORMAL, &sp) == -1 )
                                        printk(KERN_INFO "TimeKeeper: Error setting policy: %d pid: %d\n", SCHED_NORMAL, task->linux_task->pid);
                                set_children_policy(task->linux_task, SCHED_NORMAL, 0);
                        	cpumask_setall(&task->linux_task->cpus_allowed);
				set_children_cpu(task->linux_task, -1); //-1 to fill cpu mask
			}
                }
		list_del(pos);
                if (&task->timer != NULL)
                {
                        ret = hrtimer_cancel( &task->timer );
                        if (ret) printk(KERN_INFO "TimeKeeper: The timer was still in use...\n");
                }
                kfree(task);
        }
        printk(KERN_INFO "TimeKeeper: Linked list deleted\n");
        for (i=0; i<number_of_heads; i++) //clean up cpu specific chains
        {
		chainhead[i] = NULL;
		chainlength[i] = 0;
		if (experiment_stopped != NOTRUNNING) {
		if (chaintask[i] != NULL && kthread_stop(chaintask[i]) )
        		{
                		printk(KERN_INFO "TimeKeeper: Stopping worker %d error\n", i);
        		}
		}
		//clean up timeline structs
       		if (experiment_type == CS) {
			curr = timelineHead[i];
			timelineHead[i] = NULL;
			tmp = curr;
			while (curr != NULL) {
				tmp = curr;
				curr = curr->next; //move to next timeline
				kthread_stop(tmp->thread);
				kfree(tmp); // remove prev timeline
			}
		}
	}

	#ifdef __x86_64
		kill(loop_task, SIGSTOP, NULL);
	#endif

        experiment_stopped = NOTRUNNING;
	experiment_type = NOTSET;
	proc_num = 0;
	exp_highest_dilation = -100000000; //reset highest_dilation
	leader_task = NULL;
	atomic_set(&running_done, 0);
	number_of_heads = 0;
	stopped_change = 0;
}

/***
Sets the experiment_stopped flag, signalling catchup_task (sync function) to stop at the end of the current round and clean up
***/
void set_clean_exp() {
	//assuming stopExperiment will not be called if still waiting for a S3F progress to return
        if (experiment_stopped == NOTRUNNING || experiment_type == CS || experiment_stopped == FROZEN) {
                //sync experiment was never started, so just clean the list
		printk(KERN_INFO "TimeKeeper: Clean up immediately..\n");
                clean_exp();
        }
        else if (experiment_stopped == RUNNING) {
                //the experiment is running, so set the flag
		printk(KERN_INFO "TimeKeeper: Wait for catchup task to run before cleanup\n");
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

	if (aTask == NULL) {
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return;
        }
        if (aTask->pid == 0) {
                return;
        }

	me = aTask;
	t = me;
	//set if for all threads
	do {
		if (t->pid != aTask->pid) {
               		t->virt_start_time = time;
               		t->freeze_time = time;
               		t->past_physical_time = 0;
               		t->past_virtual_time = 0;
               		t->wakeup_time = 0;
			}
		} while_each_thread(me, t);


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
                taskRecurse->wakeup_time = 0;
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
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return;
        }
        if (aTask->pid == 0) {
                return;
        }

	me = aTask;
	t = me;
	//set policy for all threads as well
	do {
		if (t->pid != aTask->pid) {
	       		sp.sched_priority = priority; //some RT priority
        		if (sched_setscheduler(t, policy, &sp) == -1 )
	        	       	printk(KERN_INFO "TimeKeeper: Error setting thread policy: %d pid: %d\n",policy,t->pid);
		}
	} while_each_thread(me, t);

        list_for_each(list, &aTask->children)
        {
                taskRecurse = list_entry(list, struct task_struct, sibling);
                if (taskRecurse->pid == 0) {
                        return;
                }
		//set children scheduling policy
	        sp.sched_priority = priority; //some RT priority
        	if (sched_setscheduler(taskRecurse, policy, &sp) == -1 )
                	printk(KERN_INFO "TimeKeeper: Error setting policy: %d pid: %d\n",policy,taskRecurse->pid);
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
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return;
        }
        if (aTask->pid == 0) {
                return;
        }

	me = aTask;
	t = me;
	//set policy for all threads as well
	do {
		if (t->pid != aTask->pid) {
			if (cpu == -1) {
				//allow all cpus
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
                	//allow all cpus
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
        if (aTask == NULL) {
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return 0;
        }
        if (aTask->pid == 0) {
		printk(KERN_INFO "TimeKeeper: in freeze, it equals 0\n");
                return 0;
        }

	me = aTask;
	t = me;
	do {
		if (t->pid != aTask->pid) {
			if (t->wakeup_time > 0 ) {
				//task already has a wakeup_time set, so its already frozen, dont need to do anything
			}
      			else if (t->freeze_time == 0) //if task is not frozen yet
               		{
       	        		t->freeze_time = time;
               			kill(t, SIGSTOP, NULL);
               		}
			else {
	               		printk(KERN_INFO "TimeKeeper: Thread already frozen %d wakeuptime %lld\n", t->pid, t->wakeup_time);
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
		if (taskRecurse->wakeup_time > 0 ) {
			//task already has a wakeup_time set, so its already frozen, dont need to do anything
		}
                else if (taskRecurse->freeze_time == 0) //if task is not frozen yet
                {
        	        taskRecurse->freeze_time = time;
                	kill(taskRecurse, SIGSTOP, dilTask);
                }
		else {
                	printk(KERN_INFO "TimeKeeper: Process already frozen %d wakeuptime %lld\n", taskRecurse->pid, taskRecurse->wakeup_time);
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
        if (aTask->linux_task->freeze_time > 0)
        {
                printk(KERN_INFO "TimeKeeper: Process already frozen %d\n", aTask->linux_task->pid);
                return -1;
        }
        do_gettimeofday(&ktv);
        now = (timeval_to_ns(&ktv));
        aTask->linux_task->freeze_time = now;
	kill(aTask->linux_task, SIGSTOP, aTask);

        freeze_children(aTask->linux_task, now);
        return 0;
}


/***
Unfreezes all children associated with a container
***/
int unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time) {
        struct list_head *list;
        struct task_struct *taskRecurse;
        struct dilation_task_struct *dilTask;
        struct task_struct *me;
        struct task_struct *t;

        if (aTask == NULL) {
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return 0;
        }
        if (aTask->pid == 0) {
		printk(KERN_INFO "TimeKeeper: pid is 0 in unfreeze\n");
                return 0;
        }

	me = aTask;
	t = me;
	do {
		if (t->pid != aTask->pid) {
               		if (t->freeze_time > 0)
               		{
				t->past_physical_time = t->past_physical_time + (time - t->freeze_time);
	        	        t->freeze_time = 0;
                		kill(t, SIGCONT, NULL);
               		}
			else {
        		        printk(KERN_INFO "TimeKeeper: Thread not frozen pid: %d dilation %d\n", t->pid, t->dilation_factor);
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
		/* //wait on alarm_time code
			if (taskRecurse->alarm_time != 0 && expected_time > taskRecurse->alarm_time) {
				kill(taskRecurse, SIGALRM, NULL);
				taskRecurse->alarm_time = 0;
				printk(KERN_INFO "TimeKeeper: Sending a signal for %d\n", taskRecurse->pid);
			}
		*/
		if (taskRecurse->wakeup_time != 0 && expected_time > taskRecurse->wakeup_time) {
			printk(KERN_INFO "TimeKeeper: Time to wake up: %lld actual time: %lld\n", taskRecurse->wakeup_time, expected_time);
			taskRecurse->virt_start_time = aTask->virt_start_time;
                	taskRecurse->freeze_time = aTask->freeze_time;
                	taskRecurse->past_physical_time = aTask->past_physical_time;
                	taskRecurse->past_virtual_time = aTask->past_virtual_time;
			taskRecurse->wakeup_time = 0;
                        kill(taskRecurse, SIGCONT, dilTask);
		}
		if (taskRecurse->wakeup_time != 0 && expected_time < taskRecurse->wakeup_time) {
			//then do nothing
		}
                else if (taskRecurse->freeze_time > 0)
                {
			taskRecurse->past_physical_time = taskRecurse->past_physical_time + (time - taskRecurse->freeze_time);
        	        taskRecurse->freeze_time = 0;
                	kill(taskRecurse, SIGCONT, dilTask);
                }
		else {
	                printk(KERN_INFO "TimeKeeper: Process not frozen pid: %d dilation %d\n", taskRecurse->pid, taskRecurse->dilation_factor);
			return -1;
		}
                if (unfreeze_children(taskRecurse, time, expected_time) == -1)
			return 0;
        }
	return 0;
}


/***
Unfreezes the container, then calls unfreeze_children to unfreeze all of the children
***/
int unfreeze_proc_exp_recurse(struct dilation_task_struct *aTask, s64 expected_time) {
        struct timeval now;
        s64 now_ns;
        if (aTask->linux_task->freeze_time == 0)
        {
        printk(KERN_INFO "TimeKeeper: Process not frozen pid: %d dilation %d in recurse\n", aTask->linux_task->pid, aTask->linux_task->dilation_factor);
                return -1;
        }
        do_gettimeofday(&now);
        now_ns = timeval_to_ns(&now);
        aTask->linux_task->past_physical_time = aTask->linux_task->past_physical_time + (now_ns - aTask->linux_task->freeze_time);
        aTask->linux_task->freeze_time = 0;
        kill(aTask->linux_task, SIGCONT, aTask);
	unfreeze_children(aTask->linux_task, now_ns, expected_time);
        return 0;
}

/*
Given a pid and a new dilation, dilate it and all of it's children
*/
void dilate_proc_recurse_exp(int pid, int new_dilation) {
struct task_struct *aTask;
        aTask = find_task_by_pid(pid);
        if (aTask != NULL) {
		change_dilation(pid, new_dilation);
                perform_on_children(aTask, change_dilation, new_dilation);
	}
}

// Gets the virtual time given a dilation_task_struct
s64 get_virtual_time(struct dilation_task_struct* task, s64 now) {
	s64 virt_time;
	virt_time = get_virtual_time_task(task->linux_task, now);
        task->curr_virt_time = virt_time;
	return virt_time;
}



