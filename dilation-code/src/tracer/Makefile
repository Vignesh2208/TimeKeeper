current_dir=$(shell pwd)
EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

libs:
	@cd libperf  && ./configure  && $(MAKE) && $(MAKE) install
	

build_tracer:
	 @gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${current_dir}/utils tracer.c utils.c utils/linkedlist.c utils/hashmap.c -o tracer -lperf -ltkapi
	 @gcc  -w tests/cmd_sender.c -o cmd_sender
	 @chmod 777 tracer
	 @chmod 777 cmd_sender
	 @mv tracer /usr/bin
	 @mv cmd_sender /usr/bin

build_tracer_debug:
	@gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${current_dir}/utils -D DEBUG tracer.c utils.c utils/linkedlist.c utils/hashmap.c -o tracer -lperf -ltkapi
	@gcc  -w tests/cmd_sender.c -o cmd_sender
	@chmod 777 tracer
	@chmod 777 cmd_sender
	@mv tracer /usr/bin
	@mv cmd_sender /usr/bins

build_tracer_test: 
	@gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${current_dir}/utils -D TEST tracer.c utils.c utils/linkedlist.c utils/hashmap.c -o tracer_test -lperf -ltkapi
	@gcc  -w tests/cmd_sender.c -o cmd_sender
	@chmod 777 tracer_test
	@chmod 777 cmd_sender
	@mv tracer_test /usr/bin
	@mv cmd_sender /usr/bin

clean: 
	@cd libperf && $(MAKE) clean
	@cd tests && $(MAKE) clean
	@rm -f *.o > /dev/null
	@rm -f /usr/bin/tracer > /dev/null
	@rm -f /usr/bin/tracer_test > /dev/null
	@rm -f /usr/bin/cmd_sender > /dev/null

build_tests:
	@cd tests && $(MAKE) build

build: libs build_tracer build_tracer_test build_tests

debug: libs build_tracer_test build_tracer_debug build_tests








