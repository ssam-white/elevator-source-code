#include <stddef.h>
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

char *get_shm_name(const char *car_name) {
	char *shm_name = malloc(sizeof(car_name) + 5);
	sprintf(shm_name, "/car%s", car_name);
	return shm_name;
}

void reset_shm(car_shared_mem *s) {
	pthread_mutex_lock(&s->mutex);
	size_t offset = offsetof(car_shared_mem, current_floor);
	memset((char *)s + offset, 0, sizeof(*s) - offset);

	strcpy(s->status, "Closed");
	strcpy(s->current_floor, "1");
	strcpy(s->destination_floor, "1");
	pthread_mutex_unlock(&s->mutex);
}

void init_shm(car_shared_mem *s) {
	pthread_mutexattr_t mutattr;
	pthread_mutexattr_init(&mutattr);
	pthread_mutexattr_setpshared(&mutattr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&s->mutex, &mutattr);
	pthread_mutexattr_destroy(&mutattr);

	pthread_condattr_t condattr;
	pthread_condattr_init(&condattr);
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&s->cond, &condattr);
	pthread_condattr_destroy(&condattr);

	reset_shm(s);
}
