# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wfloat-equal -Wpointer-arith \
         -Wstrict-prototypes -Wformat=2 -Wcast-align -Wnull-dereference -Wmissing-prototypes \
         -Wmissing-declarations -Wunreachable-code -Wundef -Wcast-qual -Wwrite-strings -g

# Source files and corresponding executables
SRC = $(wildcard *.c)
EXE = $(SRC:.c=)

# Directories
SRC_DIR = src
INCLUDE_DIR = include

# Default target (build all executables)
all: $(EXE)

# Pattern rule for compiling each .c file into an executable
%: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

call: $(SRC_DIR)/call.c $(SRC_DIR)/globals.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -o call $(SRC_DIR)/call.c $(SRC_DIR)/globals.c

# Clean up object files and executables
clean:
	rm -f $(EXE) *.o
