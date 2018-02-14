EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
KBUILD_CFLAGS += -w
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules

obj-m:= TimeKeeper.o
TimeKeeper-objs := ../src/core/dilation_module.o ../src/core/general_commands.o ../src/core/sync_experiment.o ../src/core/s3f_sync_experiment.o ../src/core/common.o ../src/core/hooked_functions.o ../src/utils/hashmap.o ../src/utils/linkedlist.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules 

clean:
	$(RM) -f *.ko *.o *.mod.c Module.symvers
