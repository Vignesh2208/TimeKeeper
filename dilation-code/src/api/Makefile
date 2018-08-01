
build_api:

	 @gcc -w -c -Wall -Werror -fpic TimeKeeper_functions.c utility_functions.c
	 @gcc -w -shared -o libtkapi.so TimeKeeper_functions.o utility_functions.o
	 @mv libtkapi.so /usr/lib
	 @chmod 0755 /usr/lib/libtkapi.so

	 @cp TimeKeeper_functions.h /usr/local/include
	 @cp utility_functions.h /usr/local/include

	 @python setup.py install

clean:

	@rm -f *.o
	@rm -f /usr/lib/libtkapi.so
	@rm -f /usr/local/include/TimeKeeper_functions.h