current_dir=$(shell pwd)

clean:
	@rm -f vfork_test > /dev/null

build:
	@gcc -w vfork_test.c -o vfork_test
	@echo ${current_dir}/vfork_test > cmds.txt
	chmod 777 vfork_test

run:
	@echo "Starting Vfork Test ..."
	tracer_test -f cmds.txt
	@echo "VFork test Finished ..."

start: run
