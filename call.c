#include <stdio.h>
#include <stdlib.h>

#include "call.h"

int main (int argc, char *argv[]) {
	if (argc != 3) {
        fprintf(stderr, "Error: Incorrect number of command line arguments. Expected 2, got %d.\n", argc - 1);
		return 1;
	}

	printf("Unable to connect to elevator system.\n");

	return 0;
}
