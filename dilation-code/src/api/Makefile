
build:

	 @gcc -c -Wall -Werror -fpic TimeKeeper_functions.c utility_functions.c
	 @gcc -shared -o libtkapi.so TimeKeeper_functions.o utility_functions.o
	 @mv libtkapi.so /usr/lib
	 @chmod 0755 /usr/lib/libtkapi.so

	 @cp TimeKeeper_functions.h /usr/local/include

clean:

	@rm -f /usr/lib/libtkapi.so
	@rm -f /usr/local/include/TimeKeeper_functions.h