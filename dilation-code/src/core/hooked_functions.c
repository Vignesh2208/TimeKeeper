#include "module.h"


unsigned long **aquire_sys_call_table(void);

asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp);
asmlinkage int sys_poll_new(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int sys_select_new(int k, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage long sys_clock_nanosleep_new(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);
asmlinkage int sys_clock_gettime_new(const clockid_t which_clock, struct timespec __user * tp);


asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp, struct timespec __user *rmtp);
asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int (*ref_sys_poll_dialated)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
asmlinkage int (*ref_sys_select)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage int (*ref_sys_select_dialated)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
asmlinkage long (*ref_sys_clock_nanosleep)(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);
asmlinkage int (*ref_sys_clock_gettime)(const clockid_t which_clock, struct timespec __user * tp);




extern struct list_head exp_list;
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

extern int kill_p(struct task_struct *killTask, int sig);
extern int experiment_stopped;
extern int experiment_type;
extern atomic_t n_active_syscalls;
extern atomic_t experiment_stopping;
extern s64 boottime;
extern atomic_t is_boottime_set;
extern wait_queue_head_t expstop_call_proc_wqueue;
extern wait_queue_head_t* syscall_control_queue;
extern wait_queue_head_t sleep_queue;
extern int * syscall_running;
extern hashmap get_tracer_by_id;
extern struct mutex exp_lock;


extern int do_dialated_poll(unsigned int nfds,  struct poll_list *list,
                            struct poll_wqueues *wait, struct task_struct * tsk);
extern int do_dialated_select(int n, fd_set_bits *fds, struct task_struct * tsk, struct poll_wqueues*  table);
extern int max_sel_fd(unsigned long n, fd_set_bits *fds, struct task_struct * tsk);

extern spinlock_t syscall_lookup_lock;
extern tracer * get_tracer_for_task(struct task_struct * aTask);

extern int is_tracer_task(struct task_struct * aTask);

s64 get_dilated_time(struct task_struct * task) {
	struct timeval tv;
	do_gettimeofday(&tv);
	s64 now = timeval_to_ns(&tv);

	if (task->virt_start_time != 0) {
		return task->freeze_time;
	}

	return now;

}



/***
Hook for system call clock nanosleep
***/
asmlinkage long
sys_clock_nanosleep_new(const clockid_t which_clock,
                        int flags, const struct timespec __user * rqtp,
                        struct timespec __user * rmtp) {

	struct task_struct *current_task;
	s64 now;
	s64 wakeup_time;
	s64 difference;
	unsigned long flag;
	struct sleep_helper_struct helper;
	struct sleep_helper_struct * sleep_helper = &helper;
	tracer * curr_tracer;
	current_task = current;
	int cpu;


	struct timespec tu;
	ktime_t rem;
	struct timespec rmt;
	rem.tv64 = 0;


	if (is_tracer_task(current) >= 0) {
		return ref_sys_clock_nanosleep(which_clock, flags, rqtp, rmtp);
	}

	if (copy_from_user(&tu, rqtp, sizeof(tu)))
		return -EFAULT;

	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET) {
		atomic_inc(&n_active_syscalls);
		now = get_dilated_time(current);
		if (flags & TIMER_ABSTIME)
			wakeup_time = timespec_to_ns(&tu);
		else
			wakeup_time = now + ((tu.tv_sec * 1000000000) + tu.tv_nsec);


		init_waitqueue_head(&sleep_helper->w_queue);
		sleep_helper->wakeup_time = wakeup_time;
		atomic_set(&sleep_helper->done, 0);
		hmap_put_abs(&sleep_process_lookup, current->pid, sleep_helper);
		curr_tracer = get_tracer_for_task(current);

		if (!curr_tracer) {
			current->virt_start_time = 0;
			current->freeze_time = 0;
			current->past_physical_time = 0;
			current->past_virtual_time = 0;
			current->wakeup_time = 0;
			hmap_remove_abs(&sleep_process_lookup, current->pid);
			goto revert_nano_sleep;
		}




		if (wakeup_time > now)
			difference = wakeup_time - now;
		else
			difference = 0;

		get_tracer_struct_read(curr_tracer);
		if (curr_tracer && difference < curr_tracer->freeze_quantum) {
			put_tracer_struct_read(curr_tracer);
			cpu = curr_tracer->cpu_assignment - 2;
			//goto skip;
		} else {
			cpu = curr_tracer->cpu_assignment - 2;
			put_tracer_struct_read(curr_tracer);
		}
		PDEBUG_E("Sys Nanosleep: PID : %d, Sleep Secs: %d, "
		         "New wake up time : %lld\n", current->pid, tu.tv_sec,
		         wakeup_time);

		while (now < wakeup_time) {
			wait_event(sleep_helper->w_queue,
			           atomic_read(&sleep_helper->done) != 0);
			atomic_set(&sleep_helper->done, 0);


			now = get_dilated_time(current);

			syscall_running[cpu] = 0;
			if (atomic_read(&experiment_stopping) == 1
			        || experiment_stopped != RUNNING) {
				wake_up_interruptible(&syscall_control_queue[cpu]);
				break;
			}
			wake_up_interruptible(&syscall_control_queue[cpu]);
		}

skip:
		hmap_remove_abs(&sleep_process_lookup, current->pid);
		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);


		rmt = ktime_to_timespec(rem);
		if (copy_to_user(rmtp, &rmt, sizeof(*rmtp)))
			return -EFAULT;

		return 0;

revert_nano_sleep:
		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);

		return ref_sys_clock_nanosleep(which_clock, flags, rqtp, rmtp);

	}
	return ref_sys_clock_nanosleep(which_clock, flags, rqtp, rmtp);
}

/***
Hook for system call clock_gettime
***/
asmlinkage int sys_clock_gettime_new(const clockid_t which_clock,
                                     struct timespec __user * tp) {


	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;
	int ret;
	struct timespec temp;
	s64 mono_time;
	unsigned long flags;
	struct timeval curr_tv;
	do_gettimeofday(&curr_tv);
	s64 undialated_time_ns = timeval_to_ns(&curr_tv);

	current_task = current;

	if (which_clock != CLOCK_REALTIME
	        && which_clock != CLOCK_MONOTONIC
	        && which_clock != CLOCK_MONOTONIC_RAW
	        && which_clock != CLOCK_REALTIME_COARSE
	        && which_clock != CLOCK_MONOTONIC_COARSE)
		return ref_sys_clock_gettime(which_clock, tp);


	ret = ref_sys_clock_gettime(CLOCK_MONOTONIC, tp);
	if (copy_from_user(&temp, tp, sizeof(tp)))
		return -EFAULT;

	mono_time = timespec_to_ns(&temp);
	if (atomic_read(&is_boottime_set) == 0) {
		atomic_set(&is_boottime_set, 1);
		boottime = undialated_time_ns - mono_time;
	}

	if (experiment_stopped == RUNNING && current->virt_start_time != NOTSET) {
		now = get_dilated_time(current);
		
		struct timespec tempStruct = ns_to_timespec(now);
		if (copy_to_user(tp, &tempStruct, sizeof(tempStruct)))
			return -EFAULT;
		return 0;
	}
	return ref_sys_clock_gettime(which_clock, tp);
}

enum hrtimer_restart sleep_fn_hrtimer(struct hrtimer_dilated *timer) {

	struct sleep_helper_struct * sleep_helper = container_of(timer,
	        struct sleep_helper_struct, timer);

	PDEBUG_V("Called Sleep hrtimer function wakeup for : %d\n",
	         sleep_helper->process_pid);

	wake_up_interruptible(&sleep_helper->w_queue);

	return HRTIMER_NORESTART;
}


/***
Hooks the sleep system call, so the process will wake up when
it reaches the experiment virtual time, not the system time
***/
asmlinkage long sys_sleep_new(struct timespec __user *rqtp,
                              struct timespec __user *rmtp) {

	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;
	s64 difference, err;
	current_task = current;
	unsigned long flags;
	int ret;
	int is_dialated = 0;
	struct sleep_helper_struct helper;
	struct sleep_helper_struct * sleep_helper = &helper;
	tracer * curr_tracer;
	int cpu;

	struct timespec tu;
	ktime_t rem;
	struct timespec rmt;

	rem.tv64 = 0;

	sleep_helper->process_pid = current->pid;


	if (is_tracer_task(current) >= 0) {
		return ref_sys_sleep(rqtp, rmtp);
	}
	if (experiment_stopped == RUNNING && current->virt_start_time != 0
	        && atomic_read(&experiment_stopping) == 0) {

		if (copy_from_user(&tu, rqtp, sizeof(tu))) {
			return -EFAULT;
		}

		atomic_inc(&n_active_syscalls);
		do_gettimeofday(&ktv);
		now = get_dilated_time(current);
		init_waitqueue_head(&sleep_helper->w_queue);


		s64 wakeup_time = now + ((tu.tv_sec * 1000000000) + tu.tv_nsec);

		if (wakeup_time > now)
			difference = wakeup_time - now;
		else
			difference = 0;
		
		atomic_set(&sleep_helper->done, 0);
		sleep_helper->wakeup_time = wakeup_time;
		hmap_put_abs(&sleep_process_lookup, current->pid, sleep_helper);

		curr_tracer = get_tracer_for_task(current);

		if (!curr_tracer) {
			current->virt_start_time = 0;
			current->freeze_time = 0;
			current->past_physical_time = 0;
			current->past_virtual_time = 0;
			current->wakeup_time = 0;
			hmap_remove_abs(&sleep_process_lookup, current->pid);
			goto revert_sleep;
		}


		
		get_tracer_struct_read(curr_tracer);
		if (curr_tracer && difference < curr_tracer->freeze_quantum) {
			put_tracer_struct_read(curr_tracer);
			cpu = curr_tracer->cpu_assignment - 2;
			//goto skip_sleep;
		} else {
			cpu = curr_tracer->cpu_assignment - 2;
			put_tracer_struct_read(curr_tracer);
		}

		PDEBUG_V("Sys Sleep: PID: %d, Cpu: %d\n", current->pid, cpu);

		PDEBUG_V("Sys Sleep: PID : %d, Sleep Secs: %llu "
		         "Nano Secs: %llu, New wake up time : %llu\n",
		         current->pid, tu.tv_sec, tu.tv_nsec, wakeup_time);

		while (now < wakeup_time) {
			wait_event(sleep_helper->w_queue,
			           atomic_read(&sleep_helper->done) != 0);
			atomic_set(&sleep_helper->done, 0);

			now = get_dilated_time(current);

			if (cpu >= 0)
				syscall_running[cpu] = 0;

			PDEBUG_V("Sys Sleep: Wokeup. PID: %d, time: %llu\n",
			         current->pid, now);
			if (atomic_read(&experiment_stopping) == 1
			        || experiment_stopped != RUNNING) {
				wake_up_interruptible(&syscall_control_queue[cpu]);
				break;
			}
			wake_up_interruptible(&syscall_control_queue[cpu]);
		}
skip_sleep:


		hmap_remove_abs(&sleep_process_lookup, current->pid);
		if(now > wakeup_time)
			err = now - wakeup_time;
		else
			err = wakeup_time - now;
		PDEBUG_V("Sys Sleep: Resumed Sleep Process Expiry %d. "
		         "Resume time = %llu. Overshoot error = %llu\n",
		         current->pid, now, err );

		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);

		rmt = ktime_to_timespec(rem);
		if (copy_to_user(rmtp, &rmt, sizeof(*rmtp)))
			return -EFAULT;
		return 0;


revert_sleep:
		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);
		return ref_sys_sleep(rqtp, rmtp);
	}
	return ref_sys_sleep(rqtp, rmtp);
}



asmlinkage int sys_select_new(int k, fd_set __user *inp,
                              fd_set __user *outp, fd_set __user *exp,
                              struct timeval __user *tvp) {


	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;
	struct timespec end_time, *to = NULL;
	int ret;
	int err = -EFAULT, fdcount, len, size;
	s64 secs_to_sleep;
	s64 nsecs_to_sleep;
	struct timeval tv;
	struct timeval rtv;
	int max_fds;
	struct fdtable *fdt;
	long stack_fds[SELECT_STACK_ALLOC / sizeof(long)];
	void * bits;
	unsigned long flags;
	s64 time_to_sleep = 0;
	int is_dialated = 0;
	struct select_helper_struct  helper;
	struct select_helper_struct * select_helper = &helper;
	tracer * curr_tracer;
	int cpu;
	int n_wakeups = 0;
	int nfds = k;
	struct poll_wqueues table;

	current_task = current;

	if (is_tracer_task(current) >= 0 || k < 0) {
		return ref_sys_select(k, inp, outp, exp, tvp);
	}

	if (experiment_stopped == RUNNING && current->virt_start_time != 0
	        && tvp != NULL && atomic_read(&experiment_stopping) == 0) {

		rcu_read_lock();
		fdt = files_fdtable(current->files);
		max_fds = fdt->max_fds;
		rcu_read_unlock();
		if (nfds > max_fds) {
			PDEBUG_E("Sys Select: Resetting nfds to %d fpr PID %d\n", max_fds, current->pid);
			nfds = max_fds;
		}

		PDEBUG_V("Sys Select: Select Process Entered: %d\n", current->pid);
		atomic_inc(&n_active_syscalls);
		is_dialated = 1;
		if (copy_from_user(&tv, tvp, sizeof(tv)))
			goto revert_select;

		secs_to_sleep = tv.tv_sec + (tv.tv_usec / USEC_PER_SEC);
		nsecs_to_sleep = (tv.tv_usec % USEC_PER_SEC) * NSEC_PER_USEC;
		time_to_sleep = (secs_to_sleep * 1000000000) + nsecs_to_sleep;



		ret = -EINVAL;

		select_helper->bits = stack_fds;
		init_waitqueue_head(&select_helper->w_queue);
		atomic_set(&select_helper->done, 0);
		select_helper->ret = -EFAULT;


		select_helper->n = nfds;
		size = FDS_BYTES(nfds);
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
		select_helper->fds.ex      = bits + 2 * size;
		select_helper->fds.res_in  = bits + 3 * size;
		select_helper->fds.res_out = bits + 4 * size;
		select_helper->fds.res_ex  = bits + 5 * size;

		if ((ret = get_fd_set(nfds, inp, select_helper->fds.in)) ||
		        (ret = get_fd_set(nfds, outp, select_helper->fds.out)) ||
		        (ret = get_fd_set(nfds, exp, select_helper->fds.ex))) {

			if (select_helper->bits != stack_fds)
				kfree(select_helper->bits);
			goto revert_select;
		}

		zero_fd_set(nfds, select_helper->fds.res_in);
		zero_fd_set(nfds, select_helper->fds.res_out);
		zero_fd_set(nfds, select_helper->fds.res_ex);

		memset(&rtv, 0, sizeof(rtv));
		copy_to_user(tvp, &rtv, sizeof(rtv));
		hmap_put_abs(&select_process_lookup, current->pid, select_helper);
		curr_tracer = get_tracer_for_task(current);
		get_tracer_struct_read(curr_tracer);
		if (curr_tracer && time_to_sleep < curr_tracer->freeze_quantum) {
			//put_tracer_struct_read(curr_tracer);
			//hmap_remove_abs(&select_process_lookup, current->pid);
			//goto revert_select;
		} else if (!curr_tracer) {
			current->virt_start_time = 0;
			current->freeze_time = 0;
			current->past_physical_time = 0;
			current->past_virtual_time = 0;
			current->wakeup_time = 0;
			put_tracer_struct_read(curr_tracer);
			hmap_remove_abs(&select_process_lookup, current->pid);
			goto revert_select;
		}

		if (curr_tracer) {
			cpu = curr_tracer->cpu_assignment - 2;
		}
		put_tracer_struct_read(curr_tracer);


		do_gettimeofday(&ktv);
		now = get_dilated_time(current);
		s64 wakeup_time;

		rcu_read_lock();
		select_helper->n = max_sel_fd(select_helper->n, &select_helper->fds, current);
		rcu_read_unlock();

		poll_initwait(&table);

		wakeup_time = now + ((secs_to_sleep * 1000000000) + nsecs_to_sleep);
		s64 sleep_time = wakeup_time - now;
		PDEBUG_V("Sys Select: Select Process Waiting %d. "
		         "Timeout sec %llu, nsec %llu, wakeup_time = %llu\n",
		         current->pid, secs_to_sleep, nsecs_to_sleep, wakeup_time);

		ret = do_dialated_select(select_helper->n,
			                 &select_helper->fds, current, &table);
		if (ret || select_helper->ret == FINISHED
		        || atomic_read(&experiment_stopping) == 1
		        || experiment_stopped != RUNNING) {
			select_helper->ret = ret;
			PDEBUG_V("Sys Select: "
			         "Select Wokeup Exiting: %d, time: %llu\n",
			         current->pid, now);
			goto next_select_step;
		} else if (time_to_sleep <= curr_tracer->freeze_quantum) {
			select_helper->ret = 0;
			goto next_select_step;
		}

	
		while (now < wakeup_time) {
			wait_event(select_helper->w_queue,
			           atomic_read(&select_helper->done) != 0);
			atomic_set(&select_helper->done, 0);
			ret = do_dialated_select(select_helper->n,
			                         &select_helper->fds, current, &table);
			now = get_dilated_time(current);
			if (cpu >= 0)
				syscall_running[cpu] = 0;

			n_wakeups ++;

			PDEBUG_V("Sys Select: Wokeup. PID: %d, time: %llu\n",
			         current->pid, now);
			if (ret || select_helper->ret == FINISHED
			        || atomic_read(&experiment_stopping) == 1
			        || experiment_stopped != RUNNING) {
				select_helper->ret = ret;
				PDEBUG_V("Sys Select: "
				         "Select Wokeup Exiting: %d, time: %llu\n",
				         current->pid, now);
				wake_up_interruptible(&syscall_control_queue[cpu]);
				break;
			}

			wake_up_interruptible(&syscall_control_queue[cpu]);
		}
		next_select_step:
		if (now > wakeup_time || ret <= 0)
			select_helper->ret = 0;

		hmap_remove_abs(&select_process_lookup, current->pid);

		poll_freewait(&table);
		s64 diff = 0;
		if (wakeup_time >  now) {
			diff = wakeup_time - now;
			if (diff != sleep_time)
			PDEBUG_V("Sys Select: Resumed Select Process Early %d. "
			         "Resume time = %llu. Overshoot error = %llu, N_wakeups = %d\n",
			         current->pid, now, diff, n_wakeups);

		} else {
			diff = now - wakeup_time;
			PDEBUG_V("Sys Select: Resumed Select Process Expiry %d. "
			         "Resume time = %llu. Undershoot error = %llu\n",
			         current->pid, now, diff );
		}

		ret = select_helper->ret;
		if (ret < 0)
			goto out;

		if (set_fd_set(nfds, inp, select_helper->fds.res_in) ||
		        set_fd_set(nfds, outp, select_helper->fds.res_out) ||
		        set_fd_set(nfds, exp, select_helper->fds.res_ex))
			ret = -EFAULT;
out:

		if (bits != stack_fds)
			kfree(select_helper->bits);

out_nofds:
		PDEBUG_V("Sys Select: Select finished PID %d\n", current->pid);
		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);
		return ret;

revert_select:
		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);
		return ref_sys_select(k, inp, outp, exp, tvp);
	}
	return ref_sys_select(k, inp, outp, exp, tvp);
}

asmlinkage int sys_poll_new(struct pollfd __user * ufds,
                            unsigned int nfds, int timeout_msecs) {

	struct timeval ktv;
	struct task_struct *current_task;
	s64 now;

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
	tracer * curr_tracer;
	int cpu;

	if (timeout_msecs <= 0) {
		return ref_sys_poll(ufds, nfds, timeout_msecs);
	}

	if (is_tracer_task(current) >= 0) {
		return ref_sys_poll(ufds, nfds, timeout_msecs);
	}


	current_task = current;
	if (experiment_stopped == RUNNING
	        && current->virt_start_time != NOTSET
	        && timeout_msecs > 0 && atomic_read(&experiment_stopping) == 0) {

		PDEBUG_V("Sys Poll: Poll Entered %d.\n", current->pid);
		atomic_inc(&n_active_syscalls);
		is_dialated = 1;

		secs_to_sleep = timeout_msecs / MSEC_PER_SEC;
		nsecs_to_sleep = (timeout_msecs % MSEC_PER_SEC) * NSEC_PER_MSEC;
		time_to_sleep = (secs_to_sleep * 1000000000) + nsecs_to_sleep;


		if (nfds > rlimit(RLIMIT_NOFILE)) {
			PDEBUG_E("Sys Poll: Poll Process Invalid");
			goto revert_poll;
		}


		poll_helper->head =
		    (struct poll_list *) kmalloc(POLL_STACK_ALLOC / sizeof(long),
		                                 GFP_KERNEL);
		if (poll_helper->head == NULL) {
			PDEBUG_E("Sys Poll: Poll Process NOMEM");
			goto revert_poll;
		}

		poll_helper->table =
		    (struct poll_wqueues *) kmalloc(sizeof(struct poll_wqueues),
		                                    GFP_KERNEL);
		if (poll_helper->table == NULL) {
			PDEBUG_E("Sys Poll: Poll Process NOMEM");
			kfree(poll_helper->head);
			goto revert_poll;
		}


		head = poll_helper->head;
		poll_helper->err = -EFAULT;
		atomic_set(&poll_helper->done, 0);
		poll_helper->walk = head;
		walk = head;
		init_waitqueue_head(&poll_helper->w_queue);


		len = (nfds < N_STACK_PPS ? nfds : N_STACK_PPS);
		todo = nfds;
		for (;;) {
			walk->next = NULL;
			walk->len = len;
			if (!len)
				break;

			if (copy_from_user(walk->entries, ufds + nfds - todo,
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
		now = get_dilated_time(current);
		s64 wakeup_time;
		wakeup_time = now + ((secs_to_sleep * 1000000000) + nsecs_to_sleep);
		PDEBUG_E("Sys Poll: Poll Process Waiting %d. "
		         "Timeout sec %llu, nsec %llu\n", current->pid,
		         secs_to_sleep, nsecs_to_sleep);

		hmap_put_abs(&poll_process_lookup, current->pid, poll_helper);
		curr_tracer = get_tracer_for_task(current);
		get_tracer_struct_read(curr_tracer);
		if (curr_tracer && time_to_sleep < curr_tracer->freeze_quantum) {
			//put_tracer_struct_read(curr_tracer);
			//hmap_remove_abs(&poll_process_lookup, current->pid);
			//goto revert_poll;
		} else if (!curr_tracer) {
			current->virt_start_time = 0;
			current->freeze_time = 0;
			current->past_physical_time = 0;
			current->past_virtual_time = 0;
			current->wakeup_time = 0;
			put_tracer_struct_read(curr_tracer);
			hmap_remove_abs(&poll_process_lookup, current->pid);
			goto revert_poll;
		}

		if (curr_tracer)
			cpu = curr_tracer->cpu_assignment - 2;
		put_tracer_struct_read(curr_tracer);

		
		poll_helper->nfds = nfds;
		err = do_dialated_poll(poll_helper->nfds, poll_helper->head,
		                       poll_helper->table, current);

		if (err || poll_helper->err == FINISHED
		        || atomic_read(&experiment_stopping) == 1
		        || experiment_stopped != RUNNING) {
			poll_helper->err = err;
			goto next_poll_step;
		
		} else if (time_to_sleep <= curr_tracer->freeze_quantum) {
			goto next_poll_step;
		}

		while (now < wakeup_time) {
			wait_event(poll_helper->w_queue,
			           atomic_read(&poll_helper->done) != 0);
			atomic_set(&poll_helper->done, 0);
			poll_helper->nfds = nfds;
			err = do_dialated_poll(poll_helper->nfds, poll_helper->head,
			                       poll_helper->table, current);

			now = get_dilated_time(current);

			if (cpu >= 0)
				syscall_running[cpu] = 0;

			PDEBUG_V("Sys Poll: Wokeup. PID: %d, time: %llu\n",
			         current->pid, now);
			if (err || poll_helper->err == FINISHED
			        || atomic_read(&experiment_stopping) == 1
			        || experiment_stopped != RUNNING) {
				poll_helper->err = err;
				PDEBUG_V("Sys Poll: Poll Wokeup Exiting: %d, time: %llu\n",
				         current->pid, now);
				wake_up_interruptible(&syscall_control_queue[cpu]);
				break;
			}
			wake_up_interruptible(&syscall_control_queue[cpu]);
		}
		next_poll_step:
		if (now >= wakeup_time)
			poll_helper->err = 0;

		hmap_remove_abs(&poll_process_lookup, current->pid);
		s64 diff = 0;
		if (wakeup_time > now) {
			diff = wakeup_time - now;
			PDEBUG_V("Sys Poll: Resumed Poll Process Early %d. "
			         "Resume time = %llu. Difference = %llu\n",
			         current->pid, now, diff );

		} else {
			diff = now - wakeup_time;
			PDEBUG_V("Sys Poll: Resumed Poll Process Expiry %d. "
			         "Resume time = %llu. Difference = %llu\n",
			         current->pid, now, diff );

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
		PDEBUG_V("Sys Poll: Poll Process Finished %d\n", current->pid);
		atomic_dec(&n_active_syscalls);

		wake_up_interruptible(&expstop_call_proc_wqueue);
		return err;


revert_poll:
		atomic_dec(&n_active_syscalls);
		wake_up_interruptible(&expstop_call_proc_wqueue);
		return ref_sys_poll(ufds, nfds, timeout_msecs);

	}
	return ref_sys_poll(ufds, nfds, timeout_msecs);
}



/***
Finds us the location of the system call table
***/
unsigned long **aquire_sys_call_table(void) {
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
