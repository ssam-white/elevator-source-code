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

call: call.c global.c tcpip.c
	$(CC) -o call call.c global.c tcpip.c $(CFLAGS)

internal: internal.c global.c
	$(CC) -o internal internal.c global.c $(CFLAGS)

car: car.c global.c
	$(CC) -o car car.c global.c $(CFLAGS)

# Clean up object files and executables
clean:
	rm -f $(EXE) *.o
