current_dir=$(shell pwd)
parent_dir=$(shell dirname "$(current_dir)")

build:
	@cd repeatability-test && $(MAKE) clean && $(MAKE) build;
	@cd sleep-test && $(MAKE) clean && $(MAKE) build;
	@cd socket-test && $(MAKE) build;
	@cd vfork-test && $(MAKE) clean && $(MAKE) build;
	@cd thread-test && $(MAKE) clean && $(MAKE) build;

run_repeatability_test:
	@@cd repeatability-test && $(MAKE) start

run_sleep_test:
	@@cd sleep-test && $(MAKE) start

run_socket_test:
	@@cd socket-test && $(MAKE) start

run_vfork_test:
	@@cd vfork-test && $(MAKE) start

run_thread_test:
	@@cd thread-test && $(MAKE) start


clean:
	@cd repeatability-test && $(MAKE) clean 
	@cd sleep-test && $(MAKE) clean
	@cd vfork-test && $(MAKE) clean
	@cd thread-test && $(MAKE) clean

