EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
KBUILD_CFLAGS += -w
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules

obj-m:= TimeKeeper.o
TimeKeeper-objs := ../src/core/module.o ../src/core/common.o ../src/core/sync_experiment.o ../src/core/hooked_functions.o ../src/core/utils.o ../src/utils/hashmap.o ../src/utils/linkedlist.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules 

clean:
	$(RM) -f *.ko *.o *.mod.c Module.symvers
