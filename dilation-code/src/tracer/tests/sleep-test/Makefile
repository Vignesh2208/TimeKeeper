current_dir=$(shell pwd)

clean:
	@rm -f test > /dev/null

build:
	@gcc -w test.c -o test 
	chmod 777 test

run:
	@echo "Starting Sleep Test ..."
	@echo ${current_dir}/test > cmds.txt
	tracer_test -f cmds.txt

	@echo "Sleep Test Finished ..."


start: run
