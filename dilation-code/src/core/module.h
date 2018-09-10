#ifndef __MODULE_H
#define __MODULE_H

#include "includes.h"
//#include "utils.h"


typedef struct ioctl_arg_struct {
	s64 round_error;
	s64 n_rounds;
	s64 round_error_sq;
} ioctl_args;

/***
The callback functions for the TimeKeeper status file
***/
ssize_t status_read(struct file *pfil, char __user *pBuf, size_t len, loff_t *p_off);
ssize_t status_write(struct file *file, const char __user *buffer, size_t count, loff_t *data);
long tk_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
loff_t tk_llseek(struct file * filp, loff_t pos, int whence);
int tk_release(struct inode *inode, struct file *filp);
static const struct file_operations proc_file_fops = {
	.read = status_read,
	.write = status_write,
	.unlocked_ioctl = tk_ioctl,
	.llseek = tk_llseek,
	.release = tk_release,
	.owner = THIS_MODULE,
};


#define STATUS_MAXSIZE 1004
#define DILATION_DIR "dilation"
#define DILATION_FILE "status"

/*
How many processors are dedicated to the experiment. My system has 8, so I set it to 6 so background tasks can run on the other 2.
This needs to be >= 2 and your system needs to have at least 4 vCPUs
*/


#define NOTSET 0 	//not set yet


#define NETLINK_USER 31

/* macros for experiment_stopped */
#define NOTRUNNING -1
#define RUNNING 0
#define FROZEN 1
#define STOPPING 2
#define NOT_INITIALIZED 0
#define INITIALIZED 1
#define BUF_MAX_SIZE 200


typedef struct sched_queue_element {
	s64 n_insns_share;
	s64 n_insns_left;
	s64 n_insns_curr_round;
	int pid;
	struct task_struct * curr_task;
} lxc_schedule_elem;


typedef struct tracer_struct {
	struct task_struct * tracer_task;
	struct task_struct * spinner_task;
	int create_spinner;
	int tracer_id;
	rwlock_t tracer_lock;
	char run_q_buffer[BUF_MAX_SIZE];
	int buf_tail_ptr;
	u32 cpu_assignment;
	u32 dilation_factor;			// Indicates relative CPU speed wrt to the reference CPU speeds
	s64 freeze_quantum;
	s64 quantum_n_insns;
	s64 round_start_virt_time;
	llist schedule_queue;
	hashmap valid_children;
	hashmap ignored_children;
	lxc_schedule_elem * last_run;
	wait_queue_head_t w_queue;
	atomic_t w_queue_control;
} tracer;



struct poll_helper_struct {
	pid_t process_pid;
	struct poll_list *head;
	struct poll_list *walk;
	struct poll_wqueues *table;
	unsigned int nfds;
	int err;
	wait_queue_head_t w_queue;
	atomic_t done;
};

struct select_helper_struct {
	pid_t process_pid;
	fd_set_bits fds;
	void *bits;
	unsigned long n;
	wait_queue_head_t w_queue;
	int ret;
	atomic_t done;
};

struct sleep_helper_struct {
	pid_t process_pid;
	wait_queue_head_t w_queue;
	atomic_t done;
	struct hrtimer_dilated timer;
};


extern int handle_gettimepid(char *);
extern int pop_schedule_list(tracer * tracer_entry);
extern void requeue_schedule_list(tracer * tracer_entry);
extern void clean_up_schedule_list(tracer * tracer_entry);
extern int schedule_list_size(tracer * tracer_entry);
extern void update_tracer_schedule_queue_elem(tracer * tracer_entry, struct task_struct * tracee);
extern void add_to_tracer_schedule_queue(tracer * tracer_entry, struct task_struct * tracee);
extern void add_process_to_schedule_queue_recurse(tracer * tracer_entry, struct task_struct * tsk);
extern void refresh_tracer_schedule_queue(tracer * tracer_entry);
extern int register_tracer_process(char * write_buffer);
extern int update_tracer_params(char * write_buffer);
extern void update_task_virtual_time(tracer * tracer_entry, struct task_struct * tsk, s64 n_insns_run);
extern void update_all_children_virtual_time(tracer * tracer_entry);
extern void update_all_tracers_virtual_time(int cpuID);
extern int handle_tracer_results(char * buffer);
extern int handle_stop_exp_cmd();
extern int handle_set_netdevice_owner_cmd(char * write_buffer);
extern int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait, struct task_struct * tsk);
extern int do_dialated_select(int n, fd_set_bits *fds, struct task_struct * tsk);
int run_usermode_tracer_spin_process(char *path, char **argv, char **envp, int wait);


#endif