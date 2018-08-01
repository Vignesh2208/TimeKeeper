#include <Python.h>
#include "TimeKeeper_definitions.h"
#include "TimeKeeper_functions.h"
#include "utility_functions.h"

int o_get_stats(ioctl_args * args){

    int fp = open("/proc/dilation/status", O_RDWR);
    int ret = 0;
    if (fp < 0) {
        printf("Error communicating with TimeKeeper\n");
        return -1;
    }


    ret = ioctl(fp, TK_IO_GET_STATS, args);
    if (ret == -1) {
        perror("ioctl");
        close(fp);
        return -1;
    }
    close(fp);
    return 0;

}

static PyObject * py_addToExp(PyObject *self, PyObject *args)
{
    float rel_cpu_speed;
    u32 n_insns;
    int ret;

    if (!PyArg_ParseTuple(args, "fl", &rel_cpu_speed, &n_insns))
        return NULL;
    ret = addToExp(rel_cpu_speed,n_insns);
    return Py_BuildValue("i", ret);
}

static PyObject * py_hello(PyObject *self, PyObject *args)
{
    int ret = 1;
    hello();
    return Py_BuildValue("i", ret);
}


static PyObject * py_addToExp_sp(PyObject *self, PyObject *args)
{
    float rel_cpu_speed;
    u32 n_insns;
    int ret;
    pid_t pid;

    if (!PyArg_ParseTuple(args, "fli", &rel_cpu_speed, &n_insns, &pid))
        return NULL;
    ret = addToExp_sp(rel_cpu_speed,n_insns,pid);
    return Py_BuildValue("i", ret);
}


static PyObject * py_update_tracer_params(PyObject *self, PyObject *args)
{
    float rel_cpu_speed;
    u32 n_insns;
    int ret;
    pid_t pid;

    if (!PyArg_ParseTuple(args, "ifl",&pid, &rel_cpu_speed, &n_insns))
        return NULL;
    ret = update_tracer_params(pid, rel_cpu_speed,n_insns);
    return Py_BuildValue("i", ret);
}


static PyObject * py_write_tracer_results(PyObject *self, PyObject *args)
{
    float rel_cpu_speed;
    u32 n_insns;
    int ret;
    pid_t pid;
    //char command[MAX_BUF_SIZ];
	//flush_buffer(command,MAX_BUF_SIZ);
    char * command;

    if (!PyArg_ParseTuple(args, "s",&command))
        return NULL;

    ret = write_tracer_results(command);
    return Py_BuildValue("i", ret);
}


static PyObject * py_set_netdevice_owner(PyObject *self, PyObject *args)
{

    int pid;
    //char intf_name[100];
    //flush_buffer(intf_name,100);
    char * intf_name;
    int ret;

    if (!PyArg_ParseTuple(args, "is", &pid, &intf_name))
        return NULL;
    //printf("Setting for Net Device: %s pid = %d\n", intf_name, pid);
    ret = set_netdevice_owner(pid,intf_name);
    return Py_BuildValue("i", ret);
}

static PyObject * py_startExp(PyObject *self, PyObject *args)
{

    int ret;

    ret = startExp();
    return Py_BuildValue("i", ret);
}


static PyObject * py_stopExp(PyObject *self, PyObject *args)
{

    int ret;

    ret = stopExp();
    return Py_BuildValue("i", ret);
}

static PyObject * py_initializeExp(PyObject *self, PyObject *args)
{

    int ret;
    int n_tracers;
    if (!PyArg_ParseTuple(args, "i", &n_tracers))
        return NULL;

    ret = initializeExp(n_tracers);
    return Py_BuildValue("i", ret);
}


static PyObject * py_progress_n_rounds(PyObject *self, PyObject *args)
{
    int n_rounds;
    int ret;

    if (!PyArg_ParseTuple(args, "i", &n_rounds))
        return NULL;
    ret = progress_n_rounds(n_rounds);
    return Py_BuildValue("i", ret);
}


static PyObject * py_progress(PyObject *self, PyObject *args)
{

    int ret;

    ret = progress();
    return Py_BuildValue("i", ret);
}

static PyObject * py_fire_timers(PyObject *self, PyObject *args)
{

    int ret;

    ret = fire_timers();
    return Py_BuildValue("i", ret);
}

static PyObject * py_synchronizeAndFreeze(PyObject *self, PyObject *args)
{
    int n_tracers;
    int ret;

    if (!PyArg_ParseTuple(args, "i", &n_tracers))
        return NULL;
    ret = synchronizeAndFreeze(n_tracers);
    return Py_BuildValue("i", ret);
}


static PyObject * py_get_experiment_stats(PyObject *self, PyObject *arg)
{
    ioctl_args args;
    args.round_error = 0;
    args.round_error_sq = 0;
    args.n_rounds = 0;
    int ret;

    
    ret = get_experiment_stats(&args);
    if(ret < 0)
    	return Py_BuildValue("[i,i,i]", ret, ret, ret);
    else
    	return Py_BuildValue("[L,L,L]", args.round_error, args.round_error_sq, args.n_rounds);
}


static PyObject * py_o_get_experiment_stats(PyObject *self, PyObject *arg)
{
    ioctl_args args;
    args.round_error = 0;
    args.round_error_sq = 0;
    args.n_rounds = 0;
    int ret;
    
    ret = o_get_stats(&args);


    if(ret < 0)
    	return Py_BuildValue("[i,i,i]", ret,ret,ret);
    else{
        printf("Return Round Error: %llu, Round error sq: %llu, N rounds: %llu\n", args.round_error, args.round_error_sq, args.n_rounds);
    	return Py_BuildValue("[L,L,L]", args.round_error, args.round_error_sq, args.n_rounds);
    }
}

static PyMethodDef timekeeper_functions_methods[] = {
   { "addToExp", py_addToExp, METH_VARARGS, NULL },
   { "addToExp_sp", py_addToExp_sp, METH_VARARGS, NULL },
   { "synchronizeAndFreeze", py_synchronizeAndFreeze, METH_VARARGS, NULL },
   { "update_tracer_params", py_update_tracer_params, METH_VARARGS, NULL },
   { "write_tracer_results", py_write_tracer_results, METH_VARARGS, NULL },
   { "set_netdevice_owner", py_set_netdevice_owner, METH_VARARGS, NULL },
   { "startExp", py_startExp, METH_VARARGS, NULL },
   { "stopExp", py_stopExp, METH_VARARGS, NULL },
   { "initializeExp", py_initializeExp, METH_VARARGS, NULL },
   { "progress_n_rounds", py_progress_n_rounds, METH_VARARGS, NULL },
   { "progress", py_progress, METH_VARARGS, NULL },
   { "fire_timers", py_fire_timers, METH_VARARGS, NULL},
   { "get_experiment_stats", py_get_experiment_stats, METH_VARARGS, NULL },
   { "hello", py_hello, METH_VARARGS, NULL },
   { "o_get_experiment_stats", py_o_get_experiment_stats, METH_VARARGS, NULL },
   {NULL, NULL, 0, NULL}
};





#if PY_MAJOR_VERSION <= 2

void inittimekeeper_functions(void)
{
    Py_InitModule3("timekeeper_functions", timekeeper_functions_methods,
                   "timekeeper functions");
}

#elif PY_MAJOR_VERSION >= 3 

static struct PyModuleDef timekeeper_api_definition = { 
    PyModuleDef_HEAD_INIT,
    "timekeeper functions",
    "A Python module that exposes timekeeper's API",
    -1, 
    timekeeper_functions_methods
};
PyMODINIT_FUNC PyInit_timekeeper_functions(void)
{
    Py_Initialize();

    return PyModule_Create(&timekeeper_api_definition);
}

#endif
