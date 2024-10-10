# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wfloat-equal -Wpointer-arith \
         -Wstrict-prototypes -Wformat=2 -Wcast-align -Wnull-dereference -Wmissing-prototypes \
         -Wmissing-declarations -Wunreachable-code -Wundef -Wcast-qual -Wwrite-strings -g

# Default target (build all executables)
all: $(EXE)

# Pattern rule for compiling each .c file into an executable
%: %.c
	$(CC) -o $@ $< $(CFLAGS)

call: call.c posix.c tcpip.c
	$(CC) -o call call.c posix.c tcpip.c $(CFLAGS)

internal: internal.c posix.c
	$(CC) -o internal internal.c posix.c $(CFLAGS)

car: car.c posix.c
	$(CC) -o car car.c posix.c $(CFLAGS)

# Clean up object files and executables
clean:
	rm -f $(EXE) *.o
