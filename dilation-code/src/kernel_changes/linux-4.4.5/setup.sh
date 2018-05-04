#!/bin/bash

curr_dir=$(pwd)
base=$(pwd)/../../..
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# ** RB ***

cur_kernel=`basename $script_dir`
echo "Going to Install Kernel : $cur_kernel"
echo "Cleaning up target (if any) : $cur_kernel"
rm "$cur_kernel.tar.gz"

sudo echo "Starting Kernel Setup. Continuing with sudo permissions."

# Adding precise repository for qt3 dependencies
echo "deb http://archive.ubuntu.com/ubuntu precise main universe multiverse" | sudo tee /etc/apt/sources.list.d/precise.list
sudo apt-get update

cd $base/scripts
echo "Compiling helper scripts"
make 2>/dev/null
echo "Copying helper scripts to /bin"
#sudo cp ping /bin/
#sudo cp print_time /bin/
sudo cp x64_synchronizer /bin/

#echo "Installing required dependencies"
sudo apt-get install git-core libncurses5 libncurses5-dev libssl-dev libelf-dev binutils-dev linux-source qt3-dev-tools libqt3-mt-dev fakeroot build-essential crash kexec-tools makedumpfile kernel-wedge kernel-package python-dev uml-utilities vtun autoconf automake1.11 lua5.2-dev flex bison bridge-utils cgroup-lite libcap-dev

sudo ln -s /etc/apparmor.d/usr.bin.lxc-start /etc/apparmor.d/disable/
sudo mkdir -p /usr/local/var/lib/lxc
sudo mkdir -p /cgroup
sudo cgroups-mount

echo "Downloading Kernel $cur_kernel source to /src"
mkdir -p /src
cd /src

ker_ver=`echo $cur_kernel | cut -d "-" -f2 | cut -d "." -f 1`
ker_src_dir="https://www.kernel.org/pub/linux/kernel/v$ker_ver.x/$cur_kernel.tar.gz"
#ker_src_dir="http://127.0.0.1:8000/pub/linux/kernel/v$ker_ver.x/$cur_kernel.tar.gz"

echo "Going to Download Kernel from : $ker_src_dir"

wget $ker_src_dir
echo  " Extracting from : $cur_kernel.tar.gz"
tar -zxvf "$cur_kernel.tar.gz"

#wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.13.1.tar.gz
#tar -zxvf linux-3.13.1.tar.gz

cd $curr_dir
#DST=/src/linux-3.13.1
DST_DIR=/src

mkdir -p $DST_DIR

if [ ! -e $DST ]; then
  echo "error: $DST not found"
  exit 1
fi


echo "Copying modfied Kernel source files"


#SRC_DIR=$script_dir
SRC_DIR=`dirname $script_dir`
#DST_DIR=/tmp

cat files_changed | while read fname
do
	# echo "Going to Copy $fname..."
	if [ -d $SRC_DIR/$fname ]
	then
		echo "Creating Directory $DST_DIR/$fname "
		mkdir -p $DST_DIR/$fname
	elif [ -f $SRC_DIR/$fname ]
	then
		echo "Copying File $SRC_DIR/$fname --> $DST_DIR/$fname"
		cp $SRC_DIR/$fname $DST_DIR/$fname
	fi
done

echo "Done. Kernel source ready for compilation."
