

clean:
	@rm -f cmd
	@rm -f alarm_test
	@rm -f *.txt


build:
	@gcc -w alarm_test.c -o alarm_test -ltkapi
	@gcc -w alarm.c -o cmd
	@chmod 777 alarm_test
	@chmod 777 cmd
