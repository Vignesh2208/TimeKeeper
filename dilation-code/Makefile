EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

.PHONY : clean
nCpus=$(shell lscpu | grep "CPU(s):" | awk -F ' ' '{print $$2}')

all: clean_all modules

clean_all: clean_build clean_core clean_utils clean_api clean_tracer

build: clean_all modules

setup_4_4_kernel: download_4_4_kernel

download_4_4_kernel:
	pushd src/kernel_changes/linux-4.4.5
	./setup.sh
	popd
	

modules: 
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules 

install:
	sudo insmod build/TimeKeeper.ko

uninstall:
	sudo rmmod build/TimeKeeper.ko


clean_build:
	@echo "Cleaning old TimeKeeper build files ..."
	@$(RM) -f build/*.ko build/*.o build/*.mod.c build/Module.symvers build/modules.order ;


clean_core:
	@echo "Cleaning old TimeKeeper core files ..."
	@$(RM) -f src/core/*.o 	

clean_utils:
	@echo "Cleaning old TimeKeeper utils files ..."
	@$(RM) -f src/utils/*.o 	

clean_api:
	@echo "Cleaning old TimeKeeper api files ..."
	@$(RM) -f src/api/*.o 	

clean_tracer:
	@echo "Cleaning tracer files ..."
	@$(RM) -f src/tracer/*.o src/tracer/utils/*.o src/tracer/tests/*.o
	pushd src/tracer/libperf
	make clean
	popd

