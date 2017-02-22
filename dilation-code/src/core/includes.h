#ifndef __INCLUDES__
#define __INCLUDES__
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
#include <linux/hashtable.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/string.h>

/* user defined headers */
#include "../utils/linkedlist.h"
#include "../utils/hashmap.h"
#include "../../scripts/TimeKeeper_definitions.h"


#define DEBUG_LEVEL_NONE 0
#define DEBUG_LEVEL_INFO 1
#define DEBUG_LEVEL_VERBOSE 2
#define DEBUG_LEVEL DEBUG_LEVEL_INFO


#define BITS_PER_LONG 32
#define POLLIN_SET (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define POLLOUT_SET (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR)
#define POLLEX_SET (POLLPRI)
#define FDS_IN(fds, n)          (fds->in + n)
#define FDS_OUT(fds, n)         (fds->out + n)
#define FDS_EX(fds, n)          (fds->ex + n)
#define BITS(fds, n)    (*FDS_IN(fds, n)|*FDS_OUT(fds, n)|*FDS_EX(fds, n))

#define MSEC_PER_SEC    1000L
#define USEC_PER_MSEC   1000L
#define NSEC_PER_USEC   1000L
#define NSEC_PER_MSEC   1000000L
#define USEC_PER_SEC    1000000L
#define NSEC_PER_SEC    1000000000L
#define FSEC_PER_SEC    1000000000000000L
#define EFAULT          14  
#define EINVAL          22
#define EINTR            4 
#define ENOMEM          12 
#define POLL_STACK_ALLOC      256
#define SELECT_STACK_ALLOC      256
#define RLIMIT_NOFILE           5       /* max number of open files */

#define N_STACK_PPS ((POLL_STACK_ALLOC - sizeof(struct poll_list))  / \
                         sizeof(struct pollfd))

#define POLLFD_PER_PAGE  ((4096-sizeof(struct poll_list)) / sizeof(struct pollfd))
#define FINISHED 2
#define GOT_RESULT -1
#define IFNAMESIZ 16

#ifdef ENABLE_LOCKING

#define acquire_irq_lock(lock,flags) \
do {															 \
	spin_lock_irqsave(lock,flags);								 \
} while(0)														 \


#define release_irq_lock(lock, flags) \
do {															 \
	spin_unlock_irqrestore(lock,flags);								 \
} while(0)	

#else

#define acquire_irq_lock(lock,flags) \
do {															 \
} while(0)														 \


#define release_irq_lock(lock, flags) \
do {															 \
} while(0)	

#endif


#endif
