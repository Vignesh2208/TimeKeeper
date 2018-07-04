current_dir=$(shell pwd)

clean:

	@rm -f thread_test > /dev/null


build:
	@gcc -w thread_test.c -o thread_test -lpthread
	@echo ${current_dir}/thread_test > cmds.txt
	chmod 777 thread_test

run:
	@echo "Starting Thread Test ..."
	tracer_test 0 cmds.txt

	@echo "Thread Test Finished ..."

run_python:
	@echo "Starting Thread Test ..."
	@echo "python thread.py" > cmds.txt
	tracer_test 0 cmds.txt
	@echo "Thread Test Finished ..."


start: run

start_python: run_python