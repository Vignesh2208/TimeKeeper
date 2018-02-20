#include "module.h"

int get_next_value (char *write_buffer);
int atoi(char *s);
struct task_struct* find_task_by_pid(unsigned int nr);
tracer * alloc_tracer_entry(uint32_t tracer_id, u32 dilation_factor);