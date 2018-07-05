EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

clean:

	@cd integration_test && make clean 
	@cd usleep_test && make clean 
	@cd socket_test && make clean
	@cd timerfd_test && make clean
	@cd alarm_test && make clean
	@cd producer_consumer_test && make clean
	@rm -f /usr/bin/run_test

build:
	@gcc -w run_test.c -o run_test -ltkapi
	@mv run_test /usr/bin
	@cd integration_test && make build
	@cd usleep_test && make build
	@cd socket_test && make build
	@cd alarm_test && make build
	@cd timerfd_test && make build
	@cd producer_consumer_test && make build

run_alarm_test:
	@cd alarm_test && echo "Running Alarm Test: " && chmod 777 run_test.sh && sudo ./run_test.sh

run_timerfd_test:
	@cd timerfd_test && echo "Running Timerfd Test: " && chmod 777 run_test.sh && sudo ./run_test.sh

run_usleep_test:
	@cd usleep_test && echo "Running usleep Test: " && chmod 777 run_test.sh && sudo ./run_test.sh

run_socket_test:
	@cd socket_test && echo "Running Socket Test: " && chmod 777 run_test.sh && sudo ./run_test.sh

run_producer_consmer_test:
	@cd producer_consumer_test && echo "Running Producer Consumer Test: " && chmod 777 run_test.sh && sudo ./run_test.sh	

run:
	@cd alarm_test && echo "Running Alarm Test: " && chmod 777 run_test.sh && sudo ./run_test.sh
	@cd timerfd_test && echo "Running Timerfd Test: " && chmod 777 run_test.sh && sudo ./run_test.sh
	@cd usleep_test && echo "Running usleep Test: " && chmod 777 run_test.sh && sudo ./run_test.sh
	@cd socket_test && echo "Running Socket Test: " && chmod 777 run_test.sh && sudo ./run_test.sh
	@cd producer_consumer_test && echo "Running Producer Consumer Test: " && chmod 777 run_test.sh && sudo ./run_test.sh


