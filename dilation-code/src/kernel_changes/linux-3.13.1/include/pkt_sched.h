#ifndef __NET_PKT_SCHED_H
#define __NET_PKT_SCHED_H

#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <net/sch_generic.h>
#include <linux/fs.h>
#include <linux/pid.h>


#include <net/netlink.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
#include <net/sock.h>





struct qdisc_walker {
	int	stop;
	int	skip;
	int	count;
	int	(*fn)(struct Qdisc *, unsigned long cl, struct qdisc_walker *);
};

#define QDISC_ALIGNTO		64
#define QDISC_ALIGN(len)	(((len) + QDISC_ALIGNTO-1) & ~(QDISC_ALIGNTO-1))

static inline void *qdisc_priv(struct Qdisc *q)
{
	return (char *) q + QDISC_ALIGN(sizeof(struct Qdisc));
}

struct task_struct * get_task_struct_from_qdisc(struct Qdisc *);

s64 get_current_dilated_time(struct task_struct *);

/* 
   Timer resolution MUST BE < 10% of min_schedulable_packet_size/bandwidth
   
   Normal IP packet size ~ 512byte, hence:

   0.5Kbyte/1Mbyte/sec = 0.5msec, so that we need 50usec timer for
   10Mbit ethernet.

   10msec resolution -> <50Kbit/sec.
   
   The result: [34]86 is not good choice for QoS router :-(

   The things are not so bad, because we may use artificial
   clock evaluated by integration of network data flow
   in the most critical places.
 */

typedef u64	psched_time_t;
typedef long	psched_tdiff_t;



struct netem_skb_cb {
        s64   time_to_send;
        ktime_t         tstamp_save;
};

static inline struct netem_skb_cb *netem_skb_cb(struct sk_buff *skb)
{
        /* we assume we can use skb next/prev/tstamp as storage for rb_node */
        qdisc_cb_private_validate(skb, sizeof(struct netem_skb_cb));
        return (struct netem_skb_cb *)qdisc_skb_cb(skb)->data;
}


/* Avoid doing 64 bit divide */
#define PSCHED_SHIFT			6
#define PSCHED_TICKS2NS(x)		((s64)(x) << PSCHED_SHIFT)
#define PSCHED_NS2TICKS(x)		((x) >> PSCHED_SHIFT)

#define PSCHED_TICKS_PER_SEC		PSCHED_NS2TICKS(NSEC_PER_SEC)
#define PSCHED_PASTPERFECT		0

static inline psched_time_t psched_get_time(void)
{
	return PSCHED_NS2TICKS(ktime_to_ns(ktime_get()));
}

static inline psched_tdiff_t
psched_tdiff_bounded(psched_time_t tv1, psched_time_t tv2, psched_time_t bound)
{
	return min(tv1 - tv2, bound);
}

struct qdisc_watchdog {
	struct hrtimer	timer;
	struct hrtimer	timer_dilated;
	struct Qdisc	*qdisc;
        struct sk_buff  *skb;
	int owner_pid;
};

struct netem_sched_data {
	/* internal t(ime)fifo qdisc uses t_root and sch->limit */
	struct rb_root t_root;

	/* optional qdisc for classful handling (NULL at netem init) */
	struct Qdisc	*qdisc;

	struct qdisc_watchdog watchdog;

	psched_tdiff_t latency;
	psched_tdiff_t jitter;

	u32 loss;
	u32 ecn;
	u32 limit;
	u32 counter;
	u32 gap;
	u32 duplicate;
	u32 reorder;
	u32 corrupt;
	u32 rate;
	s32 packet_overhead;
	u32 cell_size;
	u32 cell_size_reciprocal;
	s32 cell_overhead;

	struct crndstate {
		u32 last;
		u32 rho;
	} delay_cor, loss_cor, dup_cor, reorder_cor, corrupt_cor;

	struct disttable {
		u32  size;
		s16 table[0];
	} *delay_dist;

	enum  {
		CLG_RANDOM,
		CLG_4_STATES,
		CLG_GILB_ELL,
	} loss_model;

	/* Correlated Loss Generation models */
	struct clgstate {
		/* state of the Markov chain */
		u8 state;

		/* 4-states and Gilbert-Elliot models */
		u32 a1;	/* p13 for 4-states or p for GE */
		u32 a2;	/* p31 for 4-states or r for GE */
		u32 a3;	/* p32 for 4-states or h for GE */
		u32 a4;	/* p14 for 4-states or 1-k for GE */
		u32 a5; /* p23 used only in 4-states */
	} clg;

};



void qdisc_watchdog_init(struct qdisc_watchdog *wd, struct Qdisc *qdisc);
void qdisc_watchdog_schedule_ns(struct qdisc_watchdog *wd, u64 expires);
void qdisc_watchdog_schedule_ns_dilated(struct qdisc_watchdog *wd, u64 expires);

static inline void qdisc_watchdog_schedule(struct qdisc_watchdog *wd,
					   psched_time_t expires)
{
	qdisc_watchdog_schedule_ns(wd, PSCHED_TICKS2NS(expires));
}


static inline void qdisc_watchdog_schedule_dilated(struct qdisc_watchdog *wd,
                                           psched_time_t expires)
{
        qdisc_watchdog_schedule_ns_dilated(wd, PSCHED_TICKS2NS(expires));
}



void qdisc_watchdog_cancel(struct qdisc_watchdog *wd);

extern struct Qdisc_ops pfifo_qdisc_ops;
extern struct Qdisc_ops bfifo_qdisc_ops;
extern struct Qdisc_ops pfifo_head_drop_qdisc_ops;

int fifo_set_limit(struct Qdisc *q, unsigned int limit);
struct Qdisc *fifo_create_dflt(struct Qdisc *sch, struct Qdisc_ops *ops,
			       unsigned int limit);

int register_qdisc(struct Qdisc_ops *qops);
int unregister_qdisc(struct Qdisc_ops *qops);
void qdisc_get_default(char *id, size_t len);
int qdisc_set_default(const char *id);

void qdisc_list_del(struct Qdisc *q);
struct Qdisc *qdisc_lookup(struct net_device *dev, u32 handle);
struct Qdisc *qdisc_lookup_class(struct net_device *dev, u32 handle);
struct qdisc_rate_table *qdisc_get_rtab(struct tc_ratespec *r,
					struct nlattr *tab);
void qdisc_put_rtab(struct qdisc_rate_table *tab);
void qdisc_put_stab(struct qdisc_size_table *tab);
void qdisc_warn_nonwc(char *txt, struct Qdisc *qdisc);
int sch_direct_xmit(struct sk_buff *skb, struct Qdisc *q,
		    struct net_device *dev, struct netdev_queue *txq,
		    spinlock_t *root_lock);

void __qdisc_run(struct Qdisc *q);

static inline void qdisc_run(struct Qdisc *q)
{
	if (qdisc_run_begin(q))
		__qdisc_run(q);
}

int tc_classify_compat(struct sk_buff *skb, const struct tcf_proto *tp,
		       struct tcf_result *res);
int tc_classify(struct sk_buff *skb, const struct tcf_proto *tp,
		struct tcf_result *res);

/* Calculate maximal size of packet seen by hard_start_xmit
   routine of this device.
 */
static inline unsigned int psched_mtu(const struct net_device *dev)
{
	return dev->mtu + dev->hard_header_len;
}

#endif
