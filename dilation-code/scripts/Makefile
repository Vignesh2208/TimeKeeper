all: 
	@mkdir -p bin
	@gcc -o bin/x64_synchronizer x64_synchronizer.c
	@gcc -o bin/x64_synchronizer_sleep x64_synchronizer_sleep.c
	@chmod 777 bin/x64_synchronizer
	@chmod 777 bin/x64_synchronizer_sleep
	@cp bin/x64_synchronizer /bin/
	@cp bin/x64_synchronizer_sleep /bin/
	

clean:
	@rm -f bin/*
	@touch bin/README
	@chmod 777 bin/README