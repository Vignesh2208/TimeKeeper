

#ifndef __UTILS_H
#define __UTILS_H

#include "module.h"

int get_next_value (char *write_buffer);
int atoi(char *s);
struct task_struct* find_task_by_pid(unsigned int nr);
tracer * alloc_tracer_entry(uint32_t tracer_id, u32 dilation_factor);
int kill_p(struct task_struct *killTask, int sig);
void get_tracer_struct_read(tracer* tracer_entry);
void put_tracer_struct_read(tracer* tracer_entry);
void get_tracer_struct_write(tracer* tracer_entry);
void put_tracer_struct_write(tracer* tracer_entry);
#endif