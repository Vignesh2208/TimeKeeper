#include <sys/time.h>

#define MAX_PAYLOAD 1024
#define NETLINK_USER 31

// General Functions **********************

typedef unsigned long u32;

// Synchronization Functions **********************

=
int addToExp(float relative_cpu_speed, u32 n_round_instructions);
int synchronizeAndFreeze(int n_expected_tracers);
int update_tracer_params(int tracer_pid, float relative_cpu_speed, u32 n_round_instructions);
int write_tracer_results(char * result);
int set_netdevice_owner(int tracer_pid, char * intf_name);
int gettimepid(int pid);

int startExp();
int stopExp();


int progress_n_rounds(int n_rounds);
int progress();
