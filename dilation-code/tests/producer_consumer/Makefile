

clean:
	@rm -f cmd
	@rm -f pc_test
	@rm -f *.txt


build:
	@gcc pc_test.c -o pc_test -ltkapi
	@gcc -pthread producer_consumer.c -o cmd
	@chmod 777 pc_test
	@chmod 777 cmd