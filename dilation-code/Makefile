EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

.PHONY : clean
nCpus=$(shell lscpu | grep "CPU(s):" | awk -F ' ' '{print $$2}')

all: clean_all modules timekeeper_scripts

clean_all: clean_build clean_scripts

build: clean_build timekeeper_scripts modules

setup_kernel: timekeeper_scripts download_kernel

download_kernel:
	@cd src/kernel_changes/linux-3.13.1; ./setup.sh;


timekeeper_scripts:
	@echo "Compiling TimeKeeper helper scripts ..."
	@cd scripts; make;

clean_scripts:
	@echo "Cleaning old TimeKeeper helper scripts ..."
	@cd scripts; make clean;	

modules:
	@python scripts/setup_module.py $(nCpus); 
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules 

install:
	sudo insmod build/TimeKeeper.ko

uninstall:
	sudo rmmod build/TimeKeeper.ko


clean_build:
	@echo "Cleaning old TimeKeeper build files ..."
	@$(RM) -f build/*.ko build/*.o src/core/*.o src/utils/*.o build/*.mod.c build/Module.symvers build/modules.order;



