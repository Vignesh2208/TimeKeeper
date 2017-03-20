#!/bin/bash

#
# This script gets any new kernel changes made that have to be pushed upstream to the git
# repository
#

curr_dir=$(pwd)
base=$(pwd)/../../..
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Getting modfied Kernel source files"



DST_DIR=$script_dir
SRC_DIR=/src/linux-3.13.1

#arch
mkdir -p $DST_DIR/arch/x86/syscalls
mkdir -p $DST_DIR/arch/x86/vdso

sudo cp -v $SRC_DIR/arch/x86/syscalls/syscall_32.tbl $DST_DIR/arch/x86/syscalls/
sudo cp -v $SRC_DIR/arch/x86/syscalls/syscall_64.tbl $DST_DIR/arch/x86/syscalls/
sudo cp -v $SRC_DIR/arch/x86/vdso/vclock_gettime.c $DST_DIR/arch/x86/vdso/
sudo cp -v $SRC_DIR/arch/x86/vdso/vdso.lds.S $DST_DIR/arch/x86/vdso/
sudo cp -v $SRC_DIR/arch/x86/vdso/vdsox32.lds.S $DST_DIR/arch/x86/vdso/

#drivers
mkdir -p $DST_DIR/drivers/net
sudo cp -v $SRC_DIR/drivers/net/loopback.c $DST_DIR/drivers/net/

#fs
mkdir -p $DST_DIR/fs
sudo cp -v $SRC_DIR/fs/select.c $DST_DIR/fs
sudo cp -v $SRC_DIR/fs/eventpoll.c $DST_DIR/fs
sudo cp -v $SRC_DIR/fs/timerfd.c $DST_DIR/fs

#include
mkdir -p $DST_DIR/include/linux
mkdir -p $DST_DIR/include/net
sudo cp -v $SRC_DIR/include/linux/init_task.h $DST_DIR/include/linux/
sudo cp -v $SRC_DIR/include/linux/netdevice.h $DST_DIR/include/linux/
sudo cp -v $SRC_DIR/include/linux/sched.h $DST_DIR/include/linux/
sudo cp -v $SRC_DIR/include/linux/syscalls.h $DST_DIR/include/linux/
sudo cp -v $SRC_DIR/include/net/pkt_sched.h $DST_DIR/include/net/

#kernel
mkdir -p $DST_DIR/kernel
sudo cp -v $SRC_DIR/kernel/hrtimer.c $DST_DIR/kernel
sudo cp -v $SRC_DIR/kernel/signal.c $DST_DIR/kernel
sudo cp -v $SRC_DIR/kernel/time.c $DST_DIR/kernel

#net
mkdir -p $DST_DIR/net
mkdir -p $DST_DIR/net/core
mkdir -p $DST_DIR/net/packet
mkdir -p $DST_DIR/net/sched

sudo cp -v $SRC_DIR/net/core/dev.c $DST_DIR/net/core/
sudo cp -v $SRC_DIR/net/packet/af_packet.c $DST_DIR/net/packet/
sudo cp -v $SRC_DIR/net/sched/sch_api.c $DST_DIR/net/sched/
sudo cp -v $SRC_DIR/net/sched/sch_netem.c $DST_DIR/net/sched/
sudo cp -v $SRC_DIR/net/socket.c $DST_DIR/net

echo "Done. Changes Ready to be committed"

