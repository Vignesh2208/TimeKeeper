#TimeKeeper
```
TimeKeeper has been tested with Ubuntu 12.04 and 14.04 32-bit and 64-bit with Linux kernel version 3.13.1

Outlined below are basic instructions. See the installation/usage guide found in the documentation directory for additional assistance.
```

## TimeKeeper configuration
```
1. Setup Kernel:

	cd dilation-code
	sudo make setup_kernel (Note: choose kexec-tools to not handle reboots)
	
	
	#The kernel_setup script will download Linux Kernel version 3.13.1 and required software dependencies, and store it in /src directory.
 	#Then it will modify the source code with the necessary changes.
	#Compile the kernel. Follow the instructions below: 
	
	[user~]$ cd /src/linux-3.13.1
	[user~]$ sudo cp -vi /boot/config-`uname -r` .config
	[user~]$ sudo make menuconfig #load from .config file and append suitable version name for new kernel
	[user~]$ sudo make -j6
	[user~]$ sudo make modules_install -j6
	[user~]$ sudo make install
	
	#reboot system and into the newly created kernel

	#Compilation Instructions obtained from: http://mitchtech.net/compile-linux-kernel-on-ubuntu-12-04-lts-detailed/.

	


2. Build TimeKeeper:
	sudo make build
	
	#Will ask you how many VPUS you want to assign to the experiment. Leave atleast 2 VCPUS for backgorund 
	#tasks. Assign the rest to the experiment. You can find number of VCPUS on your system using the command
	#lscpu.
	#The build output is located in build/ directory which would contain the output TimeKeeper kernel module
	#Compiled helper scripts will be located in the scripts directory

3. Install TimeKeeper:
	sudo make install


From here, TimeKeeper should be installed, loaded into the Linux Kernel, and good to go!
```
