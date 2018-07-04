
## TimeKeeper-4.0
```
TimeKeeper-4.0 is a work in progress. There are several changes which are planned from the previous version. Most notably, it will use a new technique
to control advancement of virtual time. Virtual time will be controlled based on the number of instructions executed by a dilated process. 

TimeKeeper-4.0 is currently under development phase and is expected to be completed by May 2018.
Refer to TODO file for currently tracked bugs.
```

```
Setup Requirements:

* Disable Transparent HugePages: (Add the following to /etc/rc.local to permanently disable them)

if test -f /sys/kernel/mm/transparent_hugepage/enabled; then
   echo never > /sys/kernel/mm/transparent_hugepage/enabled
fi
if test -f /sys/kernel/mm/transparent_hugepage/defrag; then
   echo never > /sys/kernel/mm/transparent_hugepage/defrag
fi

* Ensure that /etc/rc.local has execute permissions.
```
