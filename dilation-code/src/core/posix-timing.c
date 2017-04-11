#include "dilation_module.h"


/***
Contains the code for acquiring the syscall table, as well as the 4 system calls Timekeeper currently hooks.
***/
extern unsigned long **aquire_sys_call_table(void);

/***
Defined in hooked_functions.c
***/
extern s64 get_dilated_time(struct task_struct * task);

extern int experiment_stopped;
extern s64 Sim_time_scale;
extern struct list_head exp_list;
extern hashmap sleep_process_lookup;
extern s64 boottime;
extern atomic_t is_boottime_set;

asmlinkage long sys_clock_nanosleep_new(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);
asmlinkage int sys_clock_gettime_new(const clockid_t which_clock, struct timespec __user * tp);
asmlinkage long (*ref_sys_clock_nanosleep)(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);
asmlinkage int (*ref_sys_clock_gettime)(const clockid_t which_clock, struct timespec __user * tp);


/***
Hook for system call clock nanosleep
***/
asmlinkage long sys_clock_nanosleep_new(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp) {

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
	s64 wakeup_time;
	unsigned long flag;
	struct sleep_helper_struct * helper;
	struct sleep_helper_struct * sleep_helper = &helper;

	acquire_irq_lock(&current->dialation_lock,flag);
	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET && current->freeze_time == 0)
	{						
		now_new = get_dilated_time(current);
		if (flags & TIMER_ABSTIME)
			wakeup_time = timespec_to_ns(rqtp);
		else
			wakeup_time = now_new + ((rqtp->tv_sec*1000000000) + rqtp->tv_nsec)*Sim_time_scale; 

		set_current_state(TASK_INTERRUPTIBLE);
		init_waitqueue_head(&sleep_helper->w_queue);
		atomic_set(&sleep_helper->done,0);
		hmap_put_abs(&sleep_process_lookup,current->pid,sleep_helper);
		release_irq_lock(&current->dialation_lock,flags);

		printk(KERN_INFO "TimeKeeper: Sys Nanosleep: PID : %d, Sleep Secs: %d, New wake up time : %lld\n",current->pid, rqtp->tv_sec, wakeup_time); 

		while(now_new < wakeup_time) {
			set_current_state(TASK_INTERRUPTIBLE);
			wait_event(sleep_helper->w_queue,atomic_read(&sleep_helper->done) != 0);
			set_current_state(TASK_RUNNING);
			atomic_set(&sleep_helper->done,0);
			now_new = get_dilated_time(current);
			if(now_new < wakeup_time){  			
			    if(current->freeze_time == 0)
			        kill(current,SIGCONT,NULL); 
			}

        }

		acquire_irq_lock(&current->dialation_lock,flag);
		hmap_remove_abs(&sleep_process_lookup, current->pid);
		release_irq_lock(&current->dialation_lock,flags);
		return 0;
			
	} 
	release_irq_lock(&current->dialation_lock,flag);

    return ref_sys_clock_nanosleep(which_clock, flags,rqtp, rmtp);

}

/***
Hook for system call clock_gettime
***/
asmlinkage int sys_clock_gettime_new(const clockid_t which_clock, struct timespec __user * tp){

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
	int ret;
	struct timespec temp;
	s64 mono_time;
	unsigned long flags;

	struct timeval curr_tv;

	do_gettimeofday(&curr_tv);
	s64 undialated_time_ns = timeval_to_ns(&curr_tv);
	//s64 boottime = 0;


	if(which_clock != CLOCK_REALTIME && which_clock != CLOCK_MONOTONIC && which_clock != CLOCK_MONOTONIC_RAW && which_clock != CLOCK_REALTIME_COARSE && which_clock != CLOCK_MONOTONIC_COARSE)
		return ref_sys_clock_gettime(which_clock,tp);


	ret = ref_sys_clock_gettime(CLOCK_MONOTONIC,tp);
	if(copy_from_user(&temp,tp,sizeof(tp)))
		return -EFAULT;

	mono_time = timespec_to_ns(&temp);
	if(atomic_read(&is_boottime_set) == 0) {
		atomic_set(&is_boottime_set,1);	
		boottime = undialated_time_ns - mono_time;
	}

	acquire_irq_lock(&current->dialation_lock,flags);
	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET)
	{	

		
		release_irq_lock(&current->dialation_lock,flags);
        list_for_each_safe(pos, n, &exp_list)
        {
            task = list_entry(pos, struct dilation_task_struct, list);
			if (find_children_info(task->linux_task, current->pid) == 1) { 
				now = get_dilated_time(task->linux_task);
				//now = now - boottime;
				printk(KERN_INFO "TimeKeeper: Sys ClockGetTime: Pid: %d. Time = %llu, boottime = %llu\n", current->pid, now, boottime);
				struct timespec tempStruct = ns_to_timespec(now);
				if(copy_to_user(tp, &tempStruct, sizeof(tempStruct)))
					return -EFAULT;
				return 0;				

			}
		}
	}
	release_irq_lock(&current->dialation_lock,flags);

	return ref_sys_clock_gettime(which_clock,tp);

}






