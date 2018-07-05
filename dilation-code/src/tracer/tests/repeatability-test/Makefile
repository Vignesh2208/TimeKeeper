current_dir=$(shell pwd)
tracer_dir=${current_dir}/../..

clean:

	@rm -f insn_tester > /dev/null
	@rm -f test > /dev/null


build:
	@gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${tracer_dir}/utils insn_tester.c ${tracer_dir}/utils/linkedlist.c ${tracer_dir}/utils/hashmap.c -o insn_tester -lperf
	@gcc test.c -o test
	@echo ${current_dir}/test > cmds.txt
	@chmod 777 test
	@chmod 777 insn_tester

run:
	@echo Starting Repeatability Test ...
	@echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
	./insn_tester cmds.txt
	@echo 2 | sudo tee /proc/sys/kernel/randomize_va_space
	@echo Repeatability Test Finished ...

