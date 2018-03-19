#include "module.h"

/*
Has basic functionality for the Kernel Module itself. It defines how the userland process communicates with the kernel module,
as well as what should happen when the kernel module is initialized and removed.
*/

extern asmlinkage int (*ref_sys_select)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
extern asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
extern asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp, struct timespec __user *rmtp);
extern asmlinkage long (*ref_sys_clock_nanosleep)(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);
extern asmlinkage int (*ref_sys_clock_gettime)(const clockid_t which_clock, struct timespec __user * tp);


int tracer_num = 0; 					// number of TRACERS in the experiment
int n_processed_tracers = 0;			// number of tracers for which a spinner has already been spawned
int EXP_CPUS = 0;
int TOTAL_CPUS = 0;
int experiment_stopped; 			 		// flag to determine state of the experiment
int experiment_status;						// INTIALIZED/NOT INITIALIZED

struct mutex exp_lock;
int *per_cpu_chain_length;
llist * per_cpu_tracer_list;

hashmap poll_process_lookup;
hashmap select_process_lookup;
hashmap sleep_process_lookup;

hashmap get_tracer_by_id;		//hashmap of <TRACER_NUMBER, TRACER_STRUCT>
hashmap get_tracer_by_pid;		//hashmap of <PID, TRACER_STRUCT>
atomic_t n_waiting_tracers = ATOMIC_INIT(0);

// Proc file declarations
static struct proc_dir_entry *dilation_dir = NULL;
static struct proc_dir_entry *dilation_file = NULL;

//address of the sys_call_table, so we can hijack certain system calls
unsigned long **sys_call_table; 

//number of CPUs in the system
int TOTAL_CPUS; 

//The register to hijack sys_call_table
unsigned long orig_cr0; 

//the socket to send data from kernel to userspace
extern struct sock *nl_sk; 

//task that loops endlessly (64-bit)
struct task_struct *loop_task;
struct task_struct * round_task = NULL;
extern wait_queue_head_t progress_sync_proc_wqueue;
extern wait_queue_head_t expstop_call_proc_wqueue;
extern int initialize_experiment_components();
extern unsigned long **aquire_sys_call_table(void);
extern int round_sync_task(void *data);


/***
Gets the PID of our synchronizer spinner task (only in 64 bit)
***/
int getSpinnerPid(struct subprocess_info *info, struct cred *new) {
        loop_task = current;
        printk(KERN_INFO "TimeKeeper: Loop Task Started. Pid: %d\n", current->pid);
        return 0;
}


/***
Gets the PID of our tracer spin task (only in 64 bit)
***/
int get_tracer_spinner_pid(struct subprocess_info *info, struct cred *new) {

		int curr_tracer_no;
		tracer * curr_tracer;
		int i = 0;
		mutex_lock(&exp_lock);
		for(i = 1 ; i <= tracer_num; i++){
			curr_tracer = hmap_get_abs(&get_tracer_by_id, i);
			if(curr_tracer && curr_tracer->create_spinner){
				curr_tracer->spinner_task = current;
				curr_tracer->create_spinner = 0;
				curr_tracer_no = i;	
				PDEBUG_A(" Tracer Spinner Started for Tracer no: %d, Spinned Pid = %d\n", curr_tracer_no, current->pid);
			} 
		}
        mutex_unlock(&exp_lock);

        bitmap_zero((&current->cpus_allowed)->bits, 8);
       	cpumask_set_cpu(1,&current->cpus_allowed);

       

        return 0;
}

/***
Hack to get 64 bit running correctly. Starts a process that will just loop while the experiment is going
on. Starts an executable specified in the path in user space from kernel space.
***/
int run_usermode_synchronizer_process(char *path, char **argv, char **envp, int wait)
{
	struct subprocess_info *info;
        gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

        info = call_usermodehelper_setup(path, argv, envp, gfp_mask, getSpinnerPid, NULL, NULL);
        if (info == NULL)
                return -ENOMEM;

        return call_usermodehelper_exec(info, wait);
}


int run_usermode_tracer_spin_process(char *path, char **argv, char **envp, int wait)
{
	struct subprocess_info *info;
        gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

        info = call_usermodehelper_setup(path, argv, envp, gfp_mask, get_tracer_spinner_pid, NULL, NULL);
        if (info == NULL)
                return -ENOMEM;

        return call_usermodehelper_exec(info, wait);
}


/***
This handles how a process from userland communicates with the kernel module. The process basically writes to:
/proc/dilation/status with a command ie, 'W', which will tell the kernel module to call the sec_clean_exp() function
***/
ssize_t status_write(struct file *file, const char __user *buffer, size_t count, loff_t *data){
	char write_buffer[STATUS_MAXSIZE];
	unsigned long buffer_size;
	int i = 0;
	int ret = 0;

 	if(count > STATUS_MAXSIZE)
	{
    	buffer_size = STATUS_MAXSIZE;
  	}
	else
	{
		buffer_size = count;

	}

	for(i = 0; i < STATUS_MAXSIZE; i++)
		write_buffer[i] = '\0';

  	if(copy_from_user(write_buffer, buffer, buffer_size))
		return -EFAULT;


	if(write_buffer[0] == REGISTER_TRACER){
		ret =  register_tracer_process(write_buffer + 2);
		PDEBUG_I("Register Tracer : %d, Return value = %d\n", current->pid, ret);
		if(ret > 0)
			ret = -255 + ret;
	}
	else if(write_buffer[0] == SYNC_AND_FREEZE){
		ret =  sync_and_freeze(write_buffer + 2);
	}
	else if(write_buffer[0] == UPDATE_TRACER_PARAMS){
		ret =  update_tracer_params(write_buffer + 2);
	}
	else if(write_buffer[0] == PROGRESS){
		ret =  resume_exp_progress();
	}
	else if(write_buffer[0] == PROGRESS_N_ROUNDS){
		ret =  progress_exp_fixed_rounds(write_buffer + 2);
	}
	else if(write_buffer[0] == START_EXP){
		ret = start_exp();
	}
	else if(write_buffer[0] == STOP_EXP){
		ret = handle_stop_exp_cmd();
	}
	else if(write_buffer[0] == TRACER_RESULTS){
		ret = handle_tracer_results(write_buffer + 2);
	}
	else if(write_buffer[0] == SET_NETDEVICE_OWNER){
		ret = handle_set_netdevice_owner_cmd(write_buffer + 2);
	}
	else if(write_buffer[0] == GETTIMEPID){
		ret = handle_gettimepid(write_buffer + 2);
	}
	else if(write_buffer[0] == INITIALIZE_EXP){
		ret = initialize_experiment_components();
	}
	else{
		printk(KERN_INFO "TIMEKEEPER: Unknown command Received. Command: %s. Buffer size: %zu.\n", write_buffer, count);
	}

	if(ret < 0)
		return ret;
	else
		return count;

	return FAIL;


	
}

/* needs to be defined, but we do not read from /proc/dilation/status so we do not do anything here */
ssize_t status_read(struct file *pfil, char __user *pBuf, size_t len, loff_t *p_off){

		int i;
		int ret;
		tracer * curr_tracer;

		PDEBUG_V("Status Read: Tracer : %d, Entered.\n", current->pid);
		if(experiment_status != INITIALIZED){
			PDEBUG_I("Status Read: Tracer : %d, Returning because experiment was not initialized\n", current->pid);
			return -1;
		}

		curr_tracer = hmap_get_abs(&get_tracer_by_pid, current->pid);
		if(!curr_tracer){
			PDEBUG_I("Status Read: Tracer : %d, not registered\n", current->pid);
			return -1;
		}
		PDEBUG_V("Status Read: Tracer : %d, Waiting for next command\n", current->pid);
		
		set_current_state(TASK_INTERRUPTIBLE);
		atomic_inc(&n_waiting_tracers);
		wake_up_interruptible(&progress_sync_proc_wqueue);
		wait_event_interruptible(curr_tracer->w_queue, atomic_read(&curr_tracer->w_queue_control) == 0);
		

		PDEBUG_V("Status Read: Tracer : %d, Resuming from wait\n", current->pid);
		

		
		if(copy_to_user(pBuf, curr_tracer->run_q_buffer, curr_tracer->buf_tail_ptr + 1 )){
			PDEBUG_I("Status Read: Tracer : %d, Resuming from wait. Error copying to user buf\n", current->pid);	
			return -EFAULT;
		}

		if(strcmp(curr_tracer->run_q_buffer, "STOP") == 0){
			// free up memory
			PDEBUG_I("Status Read: Tracer: %d, STOPPING\n", current->pid);
			if(curr_tracer->spinner_task != NULL){
				//kill_p(curr_tracer->spinner_task, SIGKILL);
				curr_tracer->spinner_task = NULL;
			}
			mutex_lock(&exp_lock);
			//hmap_remove_abs(&get_tracer_by_id, curr_tracer->tracer_id);
			//hmap_remove_abs(&get_tracer_by_pid, current->pid);
			ret = curr_tracer->buf_tail_ptr;
			kfree(curr_tracer);
			mutex_unlock(&exp_lock);

			/*if(atomic_read(&n_waiting_tracers) == 0){
				cleanup_experiment_components();
			}*/
			atomic_dec(&n_waiting_tracers);
			wake_up_interruptible(&expstop_call_proc_wqueue);

			return ret;

		}
		atomic_dec(&n_waiting_tracers);
		ret = curr_tracer->buf_tail_ptr;
		PDEBUG_V("Status Read: Tracer: %d, Returning value: %d\n", current->pid, ret);
        return ret;
}

/***
This function gets executed when the kernel module is loaded. It creates the file for process -> kernel module communication,
sets up mutexes, timers, and hooks the system call table.
***/
int __init my_module_init(void)
{
	int i;

   	PDEBUG_A(" Loading TimeKeeper MODULE\n");

	/* Set up TimeKeeper status file in /proc */
  	dilation_dir = proc_mkdir_mode(DILATION_DIR, 0555, NULL);
  	if(dilation_dir == NULL){
	    remove_proc_entry(DILATION_DIR, NULL);
   		PDEBUG_E(" Error: Could not initialize /proc/%s\n", DILATION_DIR);
   		return -ENOMEM;
  	}

  	PDEBUG_A(" /proc/%s created\n", DILATION_DIR);
  	//dilation_file = proc_create(DILATION_FILE, 0660, dilation_dir,&proc_file_fops);
	dilation_file = proc_create(DILATION_FILE, 0666, NULL,&proc_file_fops);

	if(dilation_file == NULL){
	    remove_proc_entry(DILATION_FILE, dilation_dir);
   		PDEBUG_E("Error: Could not initialize /proc/%s/%s\n", DILATION_DIR, DILATION_FILE);
   		return -ENOMEM;
  	}
	PDEBUG_A(" /proc/%s/%s created\n", DILATION_DIR, DILATION_FILE);

	/* If it is 64-bit, initialize the looping script */
	#ifdef __x86_64
		char *argv[] = { "/bin/x64_synchronizer", NULL };
	    static char *envp[] = {
        	"HOME=/",
	        "TERM=linux",
        	"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
	    run_usermode_synchronizer_process( argv[0], argv, envp, UMH_NO_WAIT );
	#endif

	/* Set up socket so Kernel can send message to userspace */
	/*struct netlink_kernel_cfg cfg = { .input = send_a_message, };
	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
	if (!nl_sk)
	{
    	PDEBUG_E("Error creating socket.\n");
    	return -ENOMEM;
	}*/


	round_task = kthread_create(&round_sync_task, NULL, "round_sync_task");
	if(!IS_ERR(round_task)) {
	    //kthread_bind(catchup_task,0);
	    wake_up_process(round_task);
	}


	/* Acquire sys_call_table, hook system calls */
    if(!(sys_call_table = aquire_sys_call_table()))
        return -1;

    if(sys_call_table){

    	PDEBUG_A(" Getting sys_call_table references\n");

		orig_cr0 = read_cr0();
		write_cr0(orig_cr0 & ~0x00010000);
		ref_sys_sleep = (void *)sys_call_table[__NR_nanosleep];        
		ref_sys_poll = (void *)sys_call_table[__NR_poll];
		ref_sys_select = (void *) sys_call_table[NR_select];
		ref_sys_clock_gettime = (void *)sys_call_table[__NR_clock_gettime];
		ref_sys_clock_nanosleep = (void *) sys_call_table[__NR_clock_nanosleep];
		write_cr0(orig_cr0 | 0x00010000);

	}


	/* Acquire number of CPUs on system */
	TOTAL_CPUS = num_online_cpus();
	PDEBUG_A(" Number of CPUS: %d\n", num_online_cpus());

	if(TOTAL_CPUS > 2 )
		EXP_CPUS = TOTAL_CPUS - 2;
	else
		EXP_CPUS = 1;
	
	PDEBUG_A(" Number of EXP_CPUS: %d\n", EXP_CPUS);


	experiment_status = NOT_INITIALIZED;
	experiment_stopped = NOTRUNNING;

	
		/* Wait to stop loop_task */
	/*#ifdef __x86_64
        	if (loop_task != NULL) {
                	kill_p(loop_task, SIGSTOP);
                	bitmap_zero((&loop_task->cpus_allowed)->bits, 8);
       				cpumask_set_cpu(1,&loop_task->cpus_allowed);
            }
        	else {
                	PDEBUG_E(" Loop_task is null??\n");
            }
	#endif*/

	PDEBUG_A(" TIMEKEEPER MODULE LOADED SUCCESSFULLY \n");


  	return 0;
}

/***
This function gets called when the kernel module is unloaded. It frees up all memory, deletes timers, and fixes
the system call table.
***/
void __exit my_module_exit(void)
{
	s64 i;



	//remove_proc_entry(DILATION_FILE, dilation_dir);
	remove_proc_entry(DILATION_FILE, NULL);
   	PDEBUG_A(" /proc/%s/%s deleted\n", DILATION_DIR, DILATION_FILE);
   	remove_proc_entry(DILATION_DIR, NULL);
   	PDEBUG_A(" /proc/%s deleted\n", DILATION_DIR);

   	//cleanup_experiment_components();

   	/*if(sys_call_table) {
	   	orig_cr0 = read_cr0();
		write_cr0(orig_cr0 & ~0x00010000);
		sys_call_table[__NR_nanosleep] = (unsigned long *)ref_sys_sleep;
		sys_call_table[__NR_clock_gettime] = (unsigned long *) ref_sys_clock_gettime;
		sys_call_table[__NR_clock_nanosleep] = (unsigned long *) ref_sys_clock_nanosleep;
		sys_call_table[__NR_poll] = (unsigned long *)ref_sys_poll;	
		sys_call_table[NR_select] = (unsigned long *)ref_sys_select;
		write_cr0(orig_cr0 | 0x00010000);

	}*/

	/* Busy wait briefly for tasks to finish -Not the best approach */
	for (i = 0; i < 1000000000; i++) {}

	if ( kthread_stop(round_task) )
    {
         PDEBUG_E(" Stopping round_task error\n");
    }
	

	for (i = 0; i < 1000000000; i++) {}

	

	/* Kill the looping task */
	#ifdef __x86_64
		if (loop_task != NULL)
			kill_p(loop_task, SIGKILL);
	#endif
   	PDEBUG_A(" MP2 MODULE UNLOADED\n");
}



/* Register the init and exit functions here so insmod can run them */
module_init(my_module_init);
module_exit(my_module_exit);

/* Required by kernel */
MODULE_LICENSE("GPL");
