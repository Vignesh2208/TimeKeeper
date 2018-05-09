EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

clean:
	@$(RM) -f bin/*
	@touch bin/README
	@chmod 777 bin/README
	@cd integration_test && make clean 
	@cd usleep_test && make clean 
	@cd socket_test && make clean
	@cd timerfd_test && make clean
	@cd alarm_test && make clean
	@cd producer_consumer && make clean

build:
	@gcc usleep.c -o bin/usleep
	@gcc testtimerfd.c -o bin/ttimerfd
	@gcc gtod.c -o bin/gtod
	@cd integration_test && make build
	@cd usleep_test && make build
	@cd socket_test && make build
	@cd alarm_test && make build
	@cd timerfd_test && make build
	@cd producer_consumer && make build
	@chmod -R 777 bin/


