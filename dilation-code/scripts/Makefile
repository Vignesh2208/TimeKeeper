all: TimeKeeper_functions.c 
	@mkdir -p bin
	@gcc -o bin/ping ping.c -w
	@gcc -o bin/utility_functions.o -c utility_functions.c -w
	@gcc -o bin/x64_synchronizer x64_synchronizer.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/print_time print_time.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-freeze timekeeper-freeze.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-unfreeze timekeeper-unfreeze.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-addToExperiment timekeeper-addToExperiment.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-startExperiment timekeeper-startExperiment.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-synchronizeAndFreeze timekeeper-synchronizeAndFreeze.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-stopExperiment timekeeper-stopExperiment.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-dilate timekeeper-dilate.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-gettime timekeeper-gettime.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-setInterval timekeeper-setInterval.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-leap timekeeper-leap.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-reset timekeeper-reset.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/timekeeper-progress timekeeper-progress.c TimeKeeper_functions.c -I. bin/utility_functions.o -w
	@gcc -o bin/random_ping random_ping.c -w
	@gcc -o bin/select_dialated select_dialated.c TimeKeeper_functions.c bin/utility_functions.o -w

clean:
	@rm -f bin/ping
	@rm -f bin/*.o
	@rm -f bin/x64_synchronizer
	@rm -f bin/print_time
	@rm -f bin/timekeeper-freeze
	@rm -f bin/timekeeper-unfreeze
	@rm -f bin/timekeeper-addToExperiment
	@rm -f bin/timekeeper-startExperiment
	@rm -f bin/timekeeper-synchronizeAndFreeze
	@rm -f bin/timekeeper-stopExperiment 
	@rm -f bin/timekeeper-dilate
	@rm -f bin/timekeeper-gettime
	@rm -f bin/timekeeper-setInterval
	@rm -f bin/timekeeper-leap
	@rm -f bin/timekeeper-reset
	@rm -f bin/timekeeper-progress
	@rm -f bin/random_ping
	@rm -f bin/select_dialated
	@rm -f bin/tmp.c