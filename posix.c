#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "posix.h"

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

void set_flag(car_shared_mem *state, uint8_t *flag, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	*flag = value;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}

void set_status(car_shared_mem *state, const char *status) {
	set_string(state, state->status, status);
}

void set_open_button(car_shared_mem *state, uint8_t value) {
	set_flag(state, &state->open_button, value);
}

void set_close_button(car_shared_mem *state, uint8_t value) {
	set_flag(state, &state->close_button, value);
}

void set_emergency_stop(car_shared_mem *state, uint8_t value) {
	set_flag(state, &state->emergency_stop, value);
}

void set_service_mode(car_shared_mem *state, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	if (value == 1) {
		state->emergency_mode = 0;
	}
	state->individual_service_mode = value;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}


void set_string(car_shared_mem *state, char *destination, const char *source, ...) {
	va_list args;

	va_start(args, source);
	pthread_mutex_lock(&state->mutex);
	vsprintf(destination, source,  args);
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
	va_end(args);
}

bool open_button_is(car_shared_mem *state, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	bool result = state->open_button == value;
	pthread_mutex_unlock(&state->mutex);
	return result;
}

bool close_button_is(car_shared_mem *state, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	bool result = state->close_button == value;
	pthread_mutex_unlock(&state->mutex);
	return result;
}

bool status_is(car_shared_mem *state, const char *status) {
	pthread_mutex_lock(&state->mutex);
	bool result = strcmp(state->status, status) == 0;
	pthread_mutex_unlock(&state->mutex);
	return result;
}

bool service_mode_is(car_shared_mem *state, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	bool result = state->individual_service_mode == value;
	pthread_mutex_unlock(&state->mutex);
	return result;
}

char *get_shm_name(const char *car_name) {
	char *shm_name = malloc(sizeof(car_name) + 5);
	sprintf(shm_name, "/car%s", car_name);
	return shm_name;
}
