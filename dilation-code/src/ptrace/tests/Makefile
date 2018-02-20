current_dir=$(shell pwd)
parent_dir=$(shell dirname "$(current_dir)")

insn_test:
	gcc -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${parent_dir}/utils insn_tester.c ${parent_dir}/utils/linkedlist.c -o insn_tester -lperf

