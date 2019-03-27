all: TimeKeeper_functions.c 
	gcc -o ping ping.c -Wall
	gcc -o utility_functions.o -c utility_functions.c -Wall
	gcc -o x64_synchronizer x64_synchronizer.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o print_time print_time.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-freeze timekeeper-freeze.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-unfreeze timekeeper-unfreeze.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-addToExperiment timekeeper-addToExperiment.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-startExperiment timekeeper-startExperiment.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-synchronizeAndFreeze timekeeper-synchronizeAndFreeze.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-stopExperiment timekeeper-stopExperiment.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-dilate timekeeper-dilate.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-gettime timekeeper-gettime.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-setInterval timekeeper-setInterval.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-leap timekeeper-leap.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-reset timekeeper-reset.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o timekeeper-progress timekeeper-progress.c TimeKeeper_functions.c -I. utility_functions.o -Wall
	gcc -o random_ping random_ping.c -Wall
