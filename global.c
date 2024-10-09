#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "global.h"

bool is_valid_floor(char *floor) {
	size_t len = strlen(floor);

	if (len > 3) {
		return false;
	}

	if (floor[0] == 'B') {
		if (len == 1) {
			return false;
		}

		for (size_t i = 1; i < len; i++) {
			if (!isdigit((unsigned char) floor[i])) {
				return false;
			}
		}
		return true;

	}

	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char) floor[i])) {
			return false;
		}
	}

	return true;
}
