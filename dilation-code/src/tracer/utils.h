#ifndef __UTILS_H
#define __UTILS_H

#include "includes.h"

void print_curr_time(char * str) ;
int run_command(char * full_command_str, pid_t * child_pid) ;
void get_next_command(int sockfd, struct sockaddr_nl* dst_addr,
                      struct msghdr* msg, struct nlmsghdr* nlh,
                      pid_t* pid, u32* n_insns);
void print_tracee_list(llist * tracee_list) ;
int create_spinner_task(pid_t * child_pid) ;
#endif
