

clean:
	@rm -f cmd
	@rm -f integration_test
	@rm -f *.txt


build:
	@gcc integration_test.c -o integration_test -ltkapi
	@gcc cmd.c -o cmd
	@chmod 777 integration_test
	@chmod 777 cmd