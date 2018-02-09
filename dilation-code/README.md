#TimeKeeper
```
TimeKeeper has been tested with Ubuntu 12.04 and 14.04 32-bit and 64-bit with Linux kernel version 3.13.1

Outlined below are basic instructions. See the installation/usage guide found in the documentation directory for additional assistance.
```

## TimeKeeper configuration
```
1. Setup Kernel:

	cd dilation-code
	sudo make setup_4_4_kernel (Note: choose kexec-tools to not handle reboots)
	
	
	#The kernel_setup script will download Linux Kernel version 4.4.5 and required software dependencies, and store it in /src directory.
 	#Then it will modify the source code with the necessary changes.
	#Compile the kernel. Follow the instructions below: 
	
	[user~]$ cd /src/linux-4.4.5
	[user~]$ sudo cp -vi /boot/config-`uname -r` .config
	[user~]$ sudo make menuconfig #load from .config file and append suitable version name for new kernel
	[user~]$ sudo make -j6
	[user~]$ sudo make modules_install -j6
	[user~]$ sudo make install
	
	#reboot system and into the newly created kernel
	#Compilation Instructions obtained from: http://mitchtech.net/compile-linux-kernel-on-ubuntu-12-04-lts-detailed/.

	#TimeKeeper can also work with linux kernel 3.13.1 which is supported by older linux systems. To use this kernel run: sudo make setup_3_1_kernel
	#and repeat the above steps.
	


2. Build TimeKeeper:
	sudo make build
	
	#Will ask you how many VPUS you want to assign to the experiment. Leave atleast 2 VCPUS for backgorund 
	#tasks. Assign the rest to the experiment. You can find number of VCPUS on your system using the command
	#lscpu.
	#The build output is located in build/ directory which would contain the output TimeKeeper kernel module
	#Compiled helper scripts will be located in the scripts directory

3. Install TimeKeeper:
	sudo make install


From here, TimeKeeper should be installed, loaded into the Linux Kernel, and good to go!.


```

## Example test cases
```
Refer to the tests directory for documentation on how to add processes to TimeKeeper's control
and how run them in virtual time.

These tests illustrate how to create simple dilated experiments consisting of multiple processes running
concurrently in virtual time and interacting with each other.

For creating a dilated experiment consisting of n processes, each process must be started and the following sequence
of steps followed:

	1. Register each process's pid with TimeKeeper
	2. Set experiment timeslice which is the duration (in virtual time) for which each process will be run during each round
	3. Set the Time Dilation factor for each process.
	4. Synchornize and Freeze all processes
	5. Start Experiment.
	6. Stop Experiment when desired.

TimeKeeper provides python and C APIs for all of the above 6 steps. Refer to tests/timekeeper_functions.py for the python API.s
```
