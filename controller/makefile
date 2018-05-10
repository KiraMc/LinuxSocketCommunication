CC = gcc
CFLAGS = -g

default: all
all :controller

controller: controller.c controller_lib.h controller_lib.c
	$(CC) $(CFLAGS) -o run controller_lib.h controller_lib.c controller.c

clean:
	rm -f run *~

