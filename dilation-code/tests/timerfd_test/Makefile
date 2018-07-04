

clean:
	@rm -f cmd
	@rm -f tfd_test
	@rm -f *.txt


build:
	@gcc -w tfd_test.c -o tfd_test -ltkapi
	@gcc -w testtimerfd.c -o cmd
	@chmod 777 tfd_test
	@chmod 777 cmd
