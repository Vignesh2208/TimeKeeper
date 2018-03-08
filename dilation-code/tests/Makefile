EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

clean:
	@$(RM) -f bin/*

build:
	gcc usleep.c -o bin/usleep
	gcc testtimerfd.c -o bin/ttimerfd
	gcc gtod.c -o bin/gtod


