
## TimeKeeper-4.0
```
TimeKeeper is a linux virtual time system based on kernel signalling techniques. It can control execution order and run time of processes and maintain a separate virtual clock for each process under its control.

A newer virtual time system called Kronos: https://github.com/Vignesh2208/Kronos has been developed which has a much more accurate virtual time advancement mechanism based on instruction counting. As such TimeKeeper is no longer actively maintained.
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

* Clone Repository into /home/${username} directory. Checkout TimeKeeper-4.0 branch

* Setup Kernel
	@cd ~/TimeKeeper/dilation_code
	@sudo make setup_kernel
	
	During the setup process do not allow kexec tools to handle kernel reboots.
	Over the course of kernel setup, a menu config would appear. 

	The following additional config steps should also be performed inside menuconfig:
		Under General setup -->
			Append a local kernel version name. For example it could be -ins-VT.
		Under kernel_hacking -->
			enable Collect kernel timers statistics
		Under Processor types and features -->
			Transparent Huge Page support -->
				Transparent Huge Page support sysfs defaults should be set to always

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
