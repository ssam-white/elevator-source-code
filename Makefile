# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wfloat-equal -Wpointer-arith \
         -Wstrict-prototypes -Wformat=2 -Wcast-align -Wnull-dereference -Wmissing-prototypes \
         -Wmissing-declarations -Wunreachable-code -Wundef -Wcast-qual -Wwrite-strings -g

# Default target (build all executables)
all: call internal car controller

# Pattern rule for compiling .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Executable targets
call: call.o posix.o tcpip.o
	$(CC) $(CFLAGS) -o $@ $^

internal: internal.o posix.o global.o
	$(CC) $(CFLAGS) -o $@ $^

car: car.o posix.o tcpip.o global.o
	$(CC) $(CFLAGS) -o $@ $^

controller: controller.o tcpip.o global.o queue.o
	$(CC) $(CFLAGS) -o $@ $^

t: test.o queue.o
	$(CC) $(CFLAGS) -o $@ $^

# Clean up object files and executables
clean:
	rm -f call internal car controller *.o
