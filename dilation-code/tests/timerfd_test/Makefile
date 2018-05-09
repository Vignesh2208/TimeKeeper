

clean:
	@rm -f cmd
	@rm -f tfd_test
	@rm -f *.txt


build:
	@gcc tfd_test.c -o tfd_test -ltkapi
	@gcc testtimerfd.c -o cmd
	@chmod 777 tfd_test
	@chmod 777 cmd