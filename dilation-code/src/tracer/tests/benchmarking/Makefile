current_dir=$(shell pwd)
tracer_dir=${current_dir}/../..

clean:

	@rm -f benchmark > /dev/null
	@rm -f test > /dev/null


build:
	@gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${tracer_dir}/utils benchmark.c ${tracer_dir}/utils/linkedlist.c ${tracer_dir}/utils/hashmap.c -o benchmark -lperf -lm
	@gcc test.c -o test
	@echo ${current_dir}/test > cmds.txt
	@chmod 777 test
	@chmod 777 benchmark

run:
	./benchmark cmds.txt

