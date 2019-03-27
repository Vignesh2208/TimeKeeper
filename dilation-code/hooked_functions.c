#include "dilation_module.h"

/*
Contains the code for acquiring the syscall table, as well as the 4 system calls Timekeeper currently hooks.
*/

unsigned long **aquire_sys_call_table(void);
asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp);
asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp, struct timespec __user *rmtp);

extern struct list_head exp_list;

extern int find_children_info(struct task_struct* aTask, int pid);
extern int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task);
extern int experiment_stopped;

/*
Hooks the sleep system call, so the process will wake up when it reaches the experiment virtual time,
not the system time
*/
asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp) {
        struct list_head *pos;
        struct list_head *n;
        struct dilation_task_struct* task;
	struct dilation_task_struct *dilTask;
        struct timeval ktv;
	struct task_struct *current_task;
        s64 now;
        s32 rem;
        s64 real_running_time;
        s64 dilated_running_time;
	current_task = current;

	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET)
	{
        	list_for_each_safe(pos, n, &exp_list)
        	{
                	task = list_entry(pos, struct dilation_task_struct, list);
			if (find_children_info(task->linux_task, current->pid) == 1) { // I think it checks if the curret task belongs to the list of tasks in the experiment (or their children)
	        	        do_gettimeofday(&ktv);
				now = timeval_to_ns(&ktv);
                	        real_running_time = now - current->virt_start_time;
                        	if (current->dilation_factor > 0) {
                			dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*1000 ,current->dilation_factor,&rem) + current->past_virtual_time;
	                                now = dilated_running_time + current->virt_start_time;
        	                }
                	        else if (current->dilation_factor < 0) {
		        	        dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*(current->dilation_factor*-1),1000,&rem) + current->past_virtual_time;
                                	now =  dilated_running_time + current->virt_start_time;
                        	}
                        	else {
                                	dilated_running_time = (real_running_time - current->past_physical_time) + current->past_virtual_time;
	                                now = dilated_running_time + current->virt_start_time;
        	                }
				current->wakeup_time = now + (rqtp->tv_sec*1000000000) + rqtp->tv_nsec; 
				dilTask = container_of(&current_task, struct dilation_task_struct, linux_task);
				kill(current, SIGSTOP, dilTask); // I think the dilation Task of the container (which was running the current task) will wake up the task using some timer.
				return 0;
			} //end if
        	} //end for loop
	} //end if
        return ref_sys_sleep(rqtp,rmtp);
}

/*
asmlinkage long sys_alarm_new(unsigned int seconds) {
        struct list_head *pos;
        struct list_head *n;
        struct dilation_task_struct* task;
	struct dilation_task_struct* dilTask;
        struct timeval ktv;
        s64 now;
        s32 rem;
        s64 real_running_time;
        s64 dilated_running_time;
	s64 target;
	target = seconds;
	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET)
	{
        	list_for_each_safe(pos, n, &exp_list)
        	{
                	task = list_entry(pos, struct dilation_task_struct, list);
			if (find_children_info(task->linux_task, current->pid) == 1) {
        	        do_gettimeofday(&ktv);
			now = timeval_to_ns(&ktv);
                        real_running_time = now - current->virt_start_time;
                        if (current->dilation_factor > 0) {
                		dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*1000 ,current->dilation_factor,&rem) + current->past_virtual_time;
                                now = dilated_running_time + current->virt_start_time;
                        }
                        else if (current->dilation_factor < 0) {
		                dilated_running_time = div_s64_rem( (real_running_time - current->past_physical_time)*(current->dilation_factor*-1),1000,&rem) + current->past_virtual_time;
                                now =  dilated_running_time + current->virt_start_time;
                        }
                        else {
                                dilated_running_time = (real_running_time - current->past_physical_time) + current->past_virtual_time;
                                now = dilated_running_time + current->virt_start_time;
                        }
			current->alarm_time = now + target*1000000000;
			printk(KERN_INFO "TimeKeeper: Set alarm for task %d %d %lld\n", current->pid, target, current->alarm_time);
			return 0;
			}
        	} //end for loop

	} //end if
	return ref_sys_alarm(seconds);
}
*/

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

