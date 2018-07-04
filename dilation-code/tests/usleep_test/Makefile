

clean:
	@rm -f usleep
	@rm -f integration_test
	@rm -f *.txt
	

build:
	@gcc -w usleep_test.c -o usleep_test -ltkapi
	@gcc -w usleep.c -o usleep
	@chmod 777 usleep_test
	@chmod 777 usleep
	
