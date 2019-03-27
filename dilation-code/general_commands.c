#include "dilation_module.h"

/*
Contains some general commands that a userland process can manually call on TimeKeeper ie: Freeze a specific process.
It is broken up into 'groups' of functions, ie: slowing down containers, freezing/unfreezing, and so forth
*/
void perform_on_children(struct task_struct *aTask, void(*action)(int,int), int val);
void yield_proc(char *write_buffer);
void yield_proc_recurse(char *write_buffer);
void freeze_proc(struct task_struct *aTask);
void unfreeze_proc(struct task_struct *aTask);
void freeze_or_unfreeze(int pid, int sig);
void change_dilation(int pid, int new_dilation);
void dilate_proc_recurse(char *write_buffer);
void dilate_proc(char *write_buffer);
void leap_proc(char *write_buffer);
void leap(int pid, int interval);
s64 get_virtual_time_task(struct task_struct* task, s64 now);

//variables needed to be able to change dilations in a synchronized experiment
extern int experiment_stopped;
extern struct list_head exp_list;
extern struct mutex exp_mutex;
extern int dilation_change;
extern void force_virtual_time(struct task_struct* aTask, s64 time);
extern s64 PRECISION;

/***
Generic function that fill given a task, a function, and a value, run the given function on all children of the given task
***/
void perform_on_children(struct task_struct *aTask, void(*action)(int,int), int val) {
        struct list_head *list;
        struct task_struct *taskRecurse;
        if (aTask == NULL) {
                printk(KERN_INFO "TimeKeeper: Task does not exist\n");
                return;
        }
        if (aTask->pid == 0) {
                return;
        }

        list_for_each(list, &aTask->children)
        {
                taskRecurse = list_entry(list, struct task_struct, sibling);
                if (taskRecurse->pid == 0) {
                        return;
                }
                action(taskRecurse->pid,val);
                perform_on_children(taskRecurse, action, val);
        }
}

//returns the current virtual time given a task struct, and the current system time
s64 get_virtual_time_task(struct task_struct* task, s64 now)
{
        s64 real_running_time;
        s64 temp_past_physical_time;
        s64 dilated_running_time;
        s32 rem;
        s64 virt_time;

        real_running_time = now - task->virt_start_time;
        temp_past_physical_time = task->past_physical_time + (now - task->freeze_time); // task->freeze_time - when it was frozen

        //get current virtual time of a task
        if (task->dilation_factor > 0)
        {
		dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*PRECISION ,task->dilation_factor,&rem) + task->past_virtual_time;
                virt_time = dilated_running_time + task->virt_start_time;
        }
        else if (task->dilation_factor < 0)
        {
		dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*(task->dilation_factor*-1), PRECISION, &rem) + task->past_virtual_time;
                virt_time =  dilated_running_time + task->virt_start_time;
        }
        else
        {
                dilated_running_time = (real_running_time - temp_past_physical_time) + task->past_virtual_time;
                virt_time = dilated_running_time + task->virt_start_time;
        }

        return virt_time;

}


//given a pid and an advancement interval, it will leap the frozen processes virtual time by the specified interval.
void leap(int pid, int interval)
{
	s64 jump;
	s64 curr_time;
	struct timeval ktv;
	struct task_struct *task;
	task = find_task_by_pid(pid);
	if (task == NULL) {
		printk(KERN_INFO "TimeKeeper: Task is null in leap, returning\n");
		return;
	}
	if (task->freeze_time == 0) {
		printk(KERN_INFO "TimeKeeper: Task is not frozen in leap, returning\n");
		return;
	}
	jump = (s64)interval * 1000; // convert microsecond interval to nanoseconds
	do_gettimeofday(&ktv);
	curr_time = timeval_to_ns(&ktv); // current system time
	curr_time = get_virtual_time_task(task, curr_time);
	force_virtual_time(task, curr_time + jump);
}

// the leap wrapper function, will simple call leap when the arguments are extracted
void leap_proc(char *write_buffer)
{
	int pid, interval, value;
	pid = atoi(write_buffer);
	value = get_next_value(write_buffer);
	interval = atoi(write_buffer + value); // gets the pid and advancement interval and advances the specified tasks's virtual time by the specified interval.
	leap(pid, interval);
}

//------------------------------------------- Freezing and Unfreezing Containers

/***
Wrapper to freeze or unfreeze a container manually
***/
void yield_proc(char *write_buffer)
{
        int pid, sig, value;
        // Get task's PID
        pid = atoi(write_buffer);
        value = get_next_value(write_buffer);
        // Get task's signal
        sig = atoi(write_buffer + value);

        //find task struct, set variable to not let timer continue
        //if sig is stop, dont allow timer to continue
        //if sig is continue, restart the timer
        freeze_or_unfreeze(pid, sig);
}

/***
Wrapper to freeze of unfreeze a container as well as all of its children manually
***/
void yield_proc_recurse(char *write_buffer) {
int pid, sig, value;
// Get task's PID
        pid = atoi(write_buffer);
        value = get_next_value(write_buffer);

        // Get task's signal
        sig = atoi(write_buffer + value);
	freeze_or_unfreeze(pid, sig);
	perform_on_children(find_task_by_pid(pid), freeze_or_unfreeze, sig);
}

/***
Freezes a container (will not run on a CPU)
***/
void freeze_proc(struct task_struct *aTask) {
        struct timeval now;
        if (aTask->freeze_time > 0)
	{
        	printk(KERN_INFO "TimeKeeper: Process already frozen\n");
        	return;
        }
        do_gettimeofday(&now);
	if (aTask->virt_start_time == 0) { //then its a regular process, so make it time dilated
		aTask->virt_start_time = (timeval_to_ns(&now));
	}
        aTask->freeze_time = (timeval_to_ns(&now));
        kill(aTask, SIGSTOP, NULL);
	return;
}

/***
Unfreezes a container (will be allowed to run on the CPU once again)
***/
void unfreeze_proc(struct task_struct *aTask) {
        struct timeval now;
        s64 now_ns;
        if (aTask->freeze_time == 0)
	{
        	printk(KERN_INFO "TimeKeeper: Process not frozen\n");
        	return;
        }
        do_gettimeofday(&now);
        now_ns = timeval_to_ns(&now);
        aTask->past_physical_time = aTask->past_physical_time + (now_ns - aTask->freeze_time);
        aTask->freeze_time = 0;
	//printk(KERN_INFO "TimeKeeper: SHOULD NOT BE IN UNFREEZE\n");
        kill(aTask, SIGCONT, NULL);
	//__thaw_task(aTask);
}

/***
Depending on the signal that was passed in to the kernel module, the freeze_proc or unfreeze_proc function will be called
***/
void freeze_or_unfreeze(int pid, int sig) {
        struct task_struct *aTask;
        aTask = find_task_by_pid(pid);
        if (aTask == NULL ) {
                printk(KERN_INFO "TimeKeeper: Process not exist\n");
                return;
        }

        if (sig==SIGSTOP) {
                freeze_proc(aTask);
        }
        else if (sig==SIGCONT) {
                unfreeze_proc(aTask);
        }
}

//------------------------------------------------ Changing Dilation of a Container

/***
Change the dilation factor of a container.
***/
void change_dilation(int pid, int new_dilation) {
        struct task_struct *aTask;
        struct timeval now_timeval;
        s64 real_running_time, dilated_running_time, now;
        s32 rem;
        aTask = find_task_by_pid(pid);
	if (aTask != NULL) {
        	do_gettimeofday(&now_timeval);
        	now = timeval_to_ns(&now_timeval);
        	//if has not been dilated before
        	if (aTask->virt_start_time == 0) {
                	aTask->virt_start_time = now;
        	}
        	real_running_time = now - aTask->virt_start_time;
        	if (aTask->dilation_factor > 0) {
			dilated_running_time = div_s64_rem((real_running_time - aTask->past_physical_time)*1000 ,aTask->dilation_factor,&rem) + aTask->past_virtual_time;
	        }
        	else if (aTask->dilation_factor < 0) {
			dilated_running_time = div_s64_rem( (real_running_time - aTask->past_physical_time)*(aTask->dilation_factor*-1),1000,&rem) + aTask->past_virtual_time;
        	}
        	else {
			dilated_running_time = (real_running_time - aTask->past_physical_time) + aTask->past_virtual_time;
        	}
	        aTask->past_physical_time = real_running_time;
	        aTask->past_virtual_time = dilated_running_time;
	        aTask->dilation_factor = new_dilation;
   		printk(KERN_INFO "TimeKeeper: Dilating new process %d %d %lld %lld\n", pid, new_dilation, real_running_time, dilated_running_time);
	}
}
// set dilation factor of all tasks in experiment list starting with the main task whose pid is given. If the exp is not running, recursilvely change the dilation factor of all children of the given pid.
void dilate_recurse(int pid, int new_dilation) {
	struct task_struct *aTask;
	struct dilation_task_struct* list_node;
	struct list_head *pos;
        struct list_head *n;
	aTask = find_task_by_pid(pid);
        if (aTask != NULL)
	{
		//see if the main task is a part of the experiment.
		//TODO: does not handle case where dilation changes of a container BEFORE the experiment starts but after it was
			//added to the experiment - should not really happen ever right?
		if (experiment_stopped == RUNNING)
		{ //if the experiment is running
		mutex_lock(&exp_mutex);
			list_for_each_safe(pos, n, &exp_list)
        		{
                		list_node = list_entry(pos, struct dilation_task_struct, list);
				if (list_node->linux_task->pid == pid)
				{ //we found that the task is running in the experiment
					list_node->newDilation = new_dilation;
					dilation_change = 1;
				}
			}
		mutex_unlock(&exp_mutex);
		}
		else {
        		change_dilation(pid, new_dilation);
        		perform_on_children(aTask, change_dilation, new_dilation);
		}
	}
}

/***Wrapper to the change_dilation function, will also change the dilations of all of the ccontainers children as well
***/
void dilate_proc_recurse(char *write_buffer) {
        int pid, new_dilation, value;

        pid = atoi(write_buffer);
        value = get_next_value(write_buffer);
        new_dilation = atoi(write_buffer + value);
        if (new_dilation == 1) {
                value += get_next_value(write_buffer + value);
                new_dilation = atoi(write_buffer + value) * -1;
        }
	dilate_recurse(pid, new_dilation);
}

/***
Wrapper to the change_dilation function
***/
void dilate_proc(char *write_buffer) {
        int pid, new_dilation, value;

        pid = atoi(write_buffer);
        value = get_next_value(write_buffer);
        new_dilation = atoi(write_buffer + value);
        if (new_dilation == 1) { // Must be some format of writing the command.
                value += get_next_value(write_buffer + value);
                new_dilation = atoi(write_buffer + value) * -1;
        }
        change_dilation(pid,new_dilation);
}
