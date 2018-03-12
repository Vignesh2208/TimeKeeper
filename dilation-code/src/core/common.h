#ifndef __COMMON_H
#define __COMMON_H


#include "includes.h"
#include "utils.h"
#include "module.h"

int pop_schedule_list(tracer * tracer_entry);
lxc_schedule_elem * schedule_list_get_head(tracer * tracer_entry);
void requeue_schedule_list(tracer * tracer_entry);
void clean_up_schedule_list(tracer * tracer_entry);
int schedule_list_size(tracer * tracer_entry);
void update_tracer_schedule_queue_elem(tracer * tracer_entry, struct task_struct * tracee);
void add_to_tracer_schedule_queue(tracer * tracer_entry, struct task_struct * tracee);
void add_process_to_schedule_queue_recurse(tracer * tracer_entry, struct task_struct * tsk);
void refresh_tracer_schedule_queue(tracer * tracer_entry);
int register_tracer_process(char * write_buffer);
int update_tracer_params(char * write_buffer);
void update_task_virtual_time(tracer * tracer_entry, struct task_struct * tsk, s64 n_insns_run);
void update_all_children_virtual_time(tracer * tracer_entry);
void update_all_tracers_virtual_time(int cpuID);
int handle_tracer_results(char * buffer);
int handle_stop_exp_cmd();
int handle_set_netdevice_owner_cmd(char * write_buffer);
int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait,struct task_struct * tsk);
int do_dialated_select(int n, fd_set_bits *fds,struct task_struct * tsk);


#endif