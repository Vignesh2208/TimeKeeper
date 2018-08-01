
## TimeKeeper-4.0
```
TimeKeeper-4.0 is a work in progress. There are several changes which are planned from the previous version. Most notably, it will use a new technique
to control advancement of virtual time. Virtual time will be controlled based on the number of instructions executed by a dilated process. 

TimeKeeper-4.0 is currently under development phase and is expected to be completed by Aug 2018.
Refer to TODO file for currently tracked bugs.
```

```
Setup Requirements:

* Ubuntu 16.04 (Has been Tested with Ubuntu 16.04.5)

* Disable Transparent HugePages: (Add the following to /etc/rc.local to permanently disable them)

if test -f /sys/kernel/mm/transparent_hugepage/enabled; then
   echo never > /sys/kernel/mm/transparent_hugepage/enabled
fi
if test -f /sys/kernel/mm/transparent_hugepage/defrag; then
   echo never > /sys/kernel/mm/transparent_hugepage/defrag
fi

* Ensure that /etc/rc.local has execute permissions.
```

```
Installation Instructions

* Clone Repository into /home/${username} directory

* Setup Kernel
	@cd ~/TimeKeeper/dilation_code
	@sudo make setup_kernel
	
	During the setup process do not allow kexec tools to handle kernel reboots.
	Over the course of kernel setup, a menu config would appear. Append a local
	kernel version name in the menu config. For example it could be linux-4.4.5-VT

* Reboot machine and boot into new kernel

* Build and Install TimeKeeper-4.0
	@cd ~/TimeKeeper/dilation_code
	@sudo make clean
	@sudo make build install
```

```
Verifying installation

* Running tests:
	Repeatability Test
	
	@cd ~/TimeKeeper/dilation-code/src/tracer/tests && sudo make run_repeatability_test

	Run All Other Tests
		-Alarm Test
		-Timerfd Test
		-Usleep Test
		-Socket Test
		-Producer Consumer Test

	@cd ~/TimeKeeper/dilation-code/tests && sudo make run

* All tests should produce a TEST_SUCCEEDED/ TEST_COMPLETED message
```
