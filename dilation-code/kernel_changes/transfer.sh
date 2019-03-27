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
