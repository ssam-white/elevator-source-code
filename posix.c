#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "posix.h"
#include "global.h"

void set_flag(car_shared_mem *state, uint8_t *flag, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	*flag = value;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}

void set_status(car_shared_mem *state, char *status) {
	pthread_mutex_lock(&state->mutex);
	strcpy(state->status, status);
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
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
	set_flag(state, &state->individual_service_mode, value);
}

bool open_button_is(car_shared_mem *state, uint8_t value) {
	pthread_mutex_lock(&state->mutex);
	bool result = state->open_button == value;
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
