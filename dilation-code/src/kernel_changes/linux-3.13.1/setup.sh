curr_dir=$(pwd)
base=$(pwd)/../../..

sudo echo "Starting Kernel Setup. Continuing with sudo permissions."

# Adding precise repository for qt3 dependencies
echo "deb http://archive.ubuntu.com/ubuntu precise main universe multiverse" | sudo tee /etc/apt/sources.list.d/precise.list
sudo apt-get update

cd $base/scripts/bin
echo "Compiling helper scripts"
make 2>/dev/null
echo "Copying helper scripts to /bin"
sudo cp ping /bin/
sudo cp print_time /bin/
sudo cp x64_synchronizer /bin/

echo "Installing required dependencies"
sudo apt-get install git-core libncurses5 libncurses5-dev libelf-dev binutils-dev linux-source qt3-dev-tools libqt3-mt-dev fakeroot build-essential crash kexec-tools makedumpfile kernel-wedge kernel-package python-dev uml-utilities vtun autoconf automake1.11 lua5.2-dev flex bison bridge-utils cgroup-lite libcap-dev

sudo ln -s /etc/apparmor.d/usr.bin.lxc-start /etc/apparmor.d/disable/
sudo mkdir -p /usr/local/var/lib/lxc
sudo mkdir -p /cgroup
sudo cgroups-mount

echo "Downloading Kernel 3.13.1 source to /src"
mkdir -p /src
cd /src
wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.13.1.tar.gz
tar -zxvf linux-3.13.1.tar.gz

cd $curr_dir
DST=/src/linux-3.13.1

if [ ! -e $DST ]; then
  echo "error: $DST not found"
  exit 1
fi

FILES="kernel/hrtimer.c \
        kernel/time.c \
        fs/select.c
        arch/x86/syscalls/syscall_32.tbl \
        arch/x86/syscalls/syscall_64.tbl \
        arch/x86/vdso/vdso.lds.S \
        arch/x86/vdso/vdsox32.lds.S \
        include/linux/init_task.h \
        include/linux/sched.h \
        include/linux/syscalls.h"

echo "Copying modfied Kernel source files"
for f in $FILES; do
  src=`basename $f`
  echo "$src -> $DST/$f"
  cp -v $src $DST/$f
done

echo "Done. Kernel source ready for compilation."
