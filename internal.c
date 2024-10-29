#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "posix.h"
#include "global.h"
#include "internal.h"

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Incorrect number of command line args.\n");
		return 1;
	}

	// initialize a new internal controller 
	icontroller_t icontroller;
	icontroller_init(&icontroller, argv[1], argv[2]);

	// attempt to connect to the car shared emory object
	if (!icontroller_connect(&icontroller)) {
		printf("Unable to access car %s.\n", icontroller.car_name);
		return 1;
	}

	switch (handle_operation(&icontroller)) {
		case I_SERVICE_MODE_ERROR:
			printf("Operation only allowed in service mode.\n");
			break;
		case I_DOORS_ERROR:
			printf("Operation not allowed while doors are open.\n");
			break;
		case I_BETWEEN_FLOORS_ERROR:
			printf("Operation not allowed while elevator is moving.\n");
			break;
		case I_INVALID_OPERATION:
			printf("Invalid operation.\n");
			break;
		default:
			break;
	}

	icontroller_deinit(&icontroller);


	return 0;
}

void icontroller_init(icontroller_t *icontroller, const char *car_name, const char *operation) {
	icontroller->car_name = car_name;
	icontroller->operation = operation;
	icontroller->shm_name = get_shm_name(car_name) + 4);
	icontroller->fd = -1;
}

void icontroller_deinit(icontroller_t *icontroller) {
	icontroller->car_name = NULL;
	icontroller->operation = NULL;

	if (icontroller->fd >= 0) {
		close(icontroller->fd);
		icontroller->fd = -1;
	}

	if (icontroller->shm_name != NULL) {
		free(icontroller->shm_name);
		icontroller->shm_name = NULL;
	}

	if (icontroller->state != NULL) {
		munmap(icontroller->state, sizeof(car_shared_mem));
		icontroller->state = NULL;
	}

}

bool icontroller_connect(icontroller_t *icontroller) {
	icontroller->fd = shm_open(icontroller->shm_name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (icontroller->fd < 0) {
		return false;
	}

	 icontroller->state = mmap(0, sizeof(*icontroller->state), PROT_READ | PROT_WRITE, MAP_SHARED, icontroller->fd, 0);
	if (icontroller->state == NULL) {
		return false;
	}

	return true;
}

int can_car_move(car_shared_mem *state) {
	if (service_mode_is(state, 0)) {
		return I_SERVICE_MODE_ERROR;
	} else if (status_is(state, "Between")) {
		return I_BETWEEN_FLOORS_ERROR;
	} else if(!status_is(state, "Closed")) {
		return I_DOORS_ERROR;
	} 
	return I_SUCCESS;
}

int handle_operation(icontroller_t *icontroller) {
	car_shared_mem *state = icontroller->state;

	if (op_is(icontroller, "open")) {
		set_open_button(state, 1);
	} else if(op_is(icontroller, "close")) {
		set_close_button(state, 1);
	} else if(op_is(icontroller, "stop")) {
		set_emergency_stop(state, 1);
	} else if (op_is(icontroller, "service_on")) {
		set_service_mode(state, 1);
	} else if (op_is(icontroller, "service_off")) {
		set_service_mode(state, 0);
	} else if (op_is(icontroller, "up") || op_is(icontroller, "down")) {
		return  can_car_move(state) < 0 ? can_car_move(state) :
			op_is(icontroller, "up") ? up(state) : down(state);
	} else {
		return I_INVALID_OPERATION;
	}
	return I_SUCCESS;
}

int up(car_shared_mem *state) {
	pthread_mutex_lock(&state->mutex);
	int result = increment_floor(state->destination_floor);;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
	return result;
}

int down(car_shared_mem *state) {
	pthread_mutex_lock(&state->mutex);
	int result = decrement_floor(state->destination_floor);;
	pthread_mutex_unlock(&state->mutex);
	return result;
}

bool op_is(icontroller_t *icontroller, const char *op) {
	pthread_mutex_lock(&icontroller->state->mutex);
	bool result = strcmp(icontroller->operation, op) == 0;
	pthread_mutex_unlock(&icontroller->state->mutex);
	return result;
}
