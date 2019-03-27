#ifndef HEADER_FILE
#define HEADER_FILE

#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <asm/siginfo.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/spinlock_types.h>

// the callback functions for the TimeKeeper status file
ssize_t status_read(struct file *pfil, char __user *pBuf, size_t len, loff_t *p_off);
ssize_t status_write(struct file *file, const char __user *buffer, size_t count, loff_t *data);
static const struct file_operations proc_file_fops = {
 .read = status_read,
 .write = status_write,
};

// This structure maintains additional info to support TimeKeeper functionality. A dilation_task_struct gets created for every
// process in an experiment
struct dilation_task_struct
{
        struct task_struct *linux_task; // the corresponding task_struct this task is associated with
        struct list_head list; // the linked list
        struct dilation_task_struct *next; // the next dilation_task_struct in the per cpu chain
        struct dilation_task_struct *prev; // the prev dilation_task_struct in the per cpu chain
        struct hrtimer timer; // the hrtimer that will be set to fire some point in the future
        short stopped; // a simple flag that gets set if the process dies
        s64 curr_virt_time; // the current virtual time of the corresponding container
        s64 running_time; // how long the container should be allowed to run in the next round
	s64 expected_time; // the expected virtual time the container should be at
	s64 wake_up_time; //if a process was told to sleep, this is the point in virtual time in which it should 'wake up'
	int newDilation; //in a synced experiment, this will store the dilation to change to
	int cpu_assignment; //-1 if it has been assigned to a CPU yet, else the CPU assignment

	s64 increment; // CS: the increment it should advance in the next round
	struct timeline* tl; // the timeline it is associated with
        struct list_head cpuList; //for CS synchronization among specific cpus
};

// An additional structure necessary to provide integration of TimeKeeper with S3F. This provides CS. Each 'timeline' is assigned to a specific
// CPU, and each timeline can have multiple containers assigned to it. Each timeline must have a unique id ( >= 0)
struct timeline
{
        int number; // the unique timeline id ( >= 0)
        struct dilation_task_struct* head; // the head of a doubly-linked list that has all the containers associated with the timeline
        int cpu_assignment; // the specific CPU this timeline is assigned to
        struct timeline* next; // points to the next timeline assigned to this cpu
        struct task_struct* user_proc; // the task_struct to send the 'finished' message to when the timeline has finished advaincing in time
	int force; // a flag to determine if the virtual time should be forced to be exact as the user expects or not
	struct task_struct* thread; // the kernel thread associated with this timeline
};

#define STATUS_MAXSIZE 1004
#define DILATION_DIR "dilation"
#define DILATION_FILE "status"

#define EXP_CPUS 4
                          // How many processors are dedicated to the experiment. My system has 8, so I set it to 6 so 
                          //background tasks can run on the other 2.
                          // This needs to be >= 2 and your system needs to have at least 4 vCPUs

#define NETLINK_USER 31

//macros for experiment_type
#define NOTSET 0 //not set yet
#define CBE 1 //best effort (ns3, core)
#define CS 2 //concurrent synchronized (S3F)

//macros for experiment_stopped
#define NOTRUNNING -1
#define RUNNING 0
#define FROZEN 1
#define STOPPING 2

//macros to determine if the experiment should be forced (CS)
#define NOFORCE 0
#define FORCE 1

void progress_exp(void);

// *** general_commands.c
extern void freeze_proc(struct task_struct *aTask);
extern void unfreeze_proc(struct task_struct *aTask);
extern void freeze_or_unfreeze(int pid, int sig);
extern void yield_proc(char *write_buffer);
extern void yield_proc_recurse(char *write_buffer);

extern void change_dilation(int pid, int new_dilation);
extern void dilate_proc_recurse(char *write_buffer);
extern void dilate_proc(char *write_buffer);
extern void timer_callback(unsigned long task_ul);

extern void leap_proc(char *write_buffer);

// *** sync_experiment.c
extern int catchup_func(void *data);
extern void core_sync_exp(void);
extern void set_clean_exp(void);
extern void add_to_exp(int pid);
extern void add_to_exp_proc(char *write_buffer);
extern void add_sim_to_exp_proc(char *write_buffer);
extern void sync_and_freeze(void);
extern void progress_exp(void);

// *** s3f_sync_experiment.c
extern void s3f_add_to_exp_proc(char *write_buffer);
extern void s3f_set_interval(char *write_buffer);
extern void s3f_progress_timeline(char *write_buffer);
extern void s3f_reset(char *write_buffer);
extern void fix_timeline_proc(char *write_buffer);

// *** hooked_functions.c
extern unsigned long **aquire_sys_call_table(void);
extern asmlinkage long sys_sleep_new(struct timespec __user *rqtp, struct timespec __user *rmtp);

// *** util.c
extern void send_a_message(int pid);
extern void send_a_message_proc(char * write_buffer);

extern int get_next_value (char *write_buffer);
extern int atoi(char *s);
extern struct task_struct* find_task_by_pid(unsigned int nr);
extern int kill(struct task_struct *killTask, int sig, struct dilation_task_struct* dilation_task);

extern void print_proc_info(char *write_buffer);
extern void print_rt_info(char *write_buffer);

extern void print_children_info_proc(char *write_buffer);
extern void print_threads_proc(char *write_buffer);


#endif

