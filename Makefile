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

call: call.c globals.c
	$(CC) -o call call.c globals.c $(CFLAGS)

internal: internal.c globals.c
	$(CC) -o internal internal.c globals.c $(CFLAGS)

# Clean up object files and executables
clean:
	rm -f $(EXE) *.o
