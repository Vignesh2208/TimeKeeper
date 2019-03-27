#!/bin/sh

cwd=$(pwd)

echo "Starting Kernel Setup"

cd scripts
make
cp ping /bin/
cp print_time /bin/
cp x64_synchronizer /bin/
cd ..
sudo apt-get install git-core libncurses5 libncurses5-dev libelf-dev asciidoc binutils-dev linux-source qt3-dev-tools libqt3-mt-dev libncurses5 libncurses5-dev fakeroot build-essential crash kexec-tools makedumpfile kernel-wedge kernel-package
mkdir /src
cd /src
wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.10.9.tar.gz
tar -zxvf linux-3.10.9.tar.gz
cd $cwd
cd kernel_changes

DST=/src/linux-3.10.9

if [ ! -e $DST ]; then
  echo "error: $DST not found"
  exit 1
fi

FILES="kernel/hrtimer.c \
        kernel/time.c \
        arch/x86/syscalls/syscall_32.tbl \
        arch/x86/syscalls/syscall_64.tbl \
        arch/x86/vdso/vdso.lds.S \
        arch/x86/vdso/vdsox32.lds.S \
        include/linux/init_task.h \
        include/linux/sched.h \
        include/linux/syscalls.h"

for f in $FILES; do
  src=`basename $f`
  #echo "$src -> $DST/$f"
  cp -v $src $DST/$f
done

cd ..

echo "Kernel Setup Complete, now compile the kernel"
