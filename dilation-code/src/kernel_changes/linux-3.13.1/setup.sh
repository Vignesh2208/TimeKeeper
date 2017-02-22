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


#for f in $FILES; do
#  src=`basename $f`
#  echo "$src -> $DST/$f"
#  cp -v $src $DST/$f
#done



SRC_DIR=/home/user/Desktop/TimeKeeper/dilation-code/src/kernel_changes/linux-3.13.1
DST_DIR=/src/linux-3.13.1

#arch
sudo cp -r $SRC_DIR/arch/syscall_32.tbl $DST_DIR/arch/x86/syscalls/
sudo cp -r $SRC_DIR/arch/syscall_64.tbl $DST_DIR/arch/x86/syscalls/
sudo cp -r $SRC_DIR/arch/vclock_gettime.c $DST_DIR/arch/x86/vdso/
sudo cp -r $SRC_DIR/arch/vdso.lds.S $DST_DIR/arch/x86/vdso/
sudo cp -r $SRC_DIR/arch/vdsox32.lds.S $DST_DIR/arch/x86/vdso/

#drivers
sudo cp -r $SRC_DIR/drivers/loopback.c $DST_DIR/drivers/net/

#fs
sudo cp -r $SRC_DIR/fs/select.c $DST_DIR/fs
sudo cp -r $SRC_DIR/fs/timerfd.c $DST_DIR/fs

#include
sudo cp -r $SRC_DIR/include/init_task.h $DST_DIR/include/linux/
sudo cp -r $SRC_DIR/include/netdevice.h $DST_DIR/include/linux/
sudo cp -r $SRC_DIR/include/sched.h $DST_DIR/include/linux/
sudo cp -r $SRC_DIR/include/syscalls.h $DST_DIR/include/linux/
sudo cp -r $SRC_DIR/include/pkt_sched.h $DST_DIR/include/net/

#kernel
sudo cp -r $SRC_DIR/kernel/hrtimer.c $DST_DIR/kernel
sudo cp -r $SRC_DIR/kernel/signal.c $DST_DIR/kernel
sudo cp -r $SRC_DIR/kernel/time.c $DST_DIR/kernel

#net
sudo cp -r $SRC_DIR/net/dev.c $DST_DIR/net/core/
sudo cp -r $SRC_DIR/net/af_packet.c $DST_DIR/net/packet/
sudo cp -r $SRC_DIR/net/sch_api.c $DST_DIR/net/sched/
sudo cp -r $SRC_DIR/net/sch_netem.c $DST_DIR/net/sched/
sudo cp -r $SRC_DIR/net/socket.c $DST_DIR/net

echo "Done. Kernel source ready for compilation."
