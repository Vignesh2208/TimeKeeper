all: TimeKeeper_functions.c 
	@mkdir -p bin
	@gcc -o bin/utility_functions.o -c utility_functions.c -w
	@gcc -o bin/x64_synchronizer x64_synchronizer.c TimeKeeper_functions.c -I. bin/utility_functions.o -w


clean:
	
	@rm -f bin/*.o
	@rm -f bin/x64_synchronizer