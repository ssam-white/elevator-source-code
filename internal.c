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

	pthread_mutex_lock(&icontroller.state->mutex);

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

	pthread_mutex_unlock(&icontroller.state->mutex);

	icontroller_deinit(&icontroller);


	return 0;
}

void icontroller_init(icontroller_t *icontroller, const char *car_name, const char *operation) {
	icontroller->car_name = car_name;
	icontroller->operation = operation;

	icontroller->shm_name = malloc(strlen(icontroller->car_name) + 4);
	sprintf(icontroller->shm_name, "/car%s", icontroller->car_name);

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

bool op_is(icontroller_t *icontroller, const char *op) {
	return strcmp(icontroller->operation, op) == 0;
}

bool status_is(car_shared_mem *state, const char *status) {
	return strcmp(state->status, status) == 0;
}

int can_car_move(car_shared_mem *state) {
	if (state->individual_service_mode == 0) {
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
		open_button_on(state);
	}	 else if(op_is(icontroller, "close")) {
		close_button_on(state);
	} else if(op_is(icontroller, "stop")) {
		stop_car(state);
	} else if (op_is(icontroller, "service_on")) {
		service_on(state);
	} else if (op_is(icontroller, "service_off")) {
		service_off(state);
	} else if (op_is(icontroller, "up") || op_is(icontroller, "down")) {
		return  can_car_move(state) < 0 ? can_car_move(state) :
			op_is(icontroller, "up") ? up(state) : down(state);
	} else {
		return I_INVALID_OPERATION;
	}
	return I_SUCCESS;
}

void open_button_on(car_shared_mem *state) {
	state->open_button = 1;
	pthread_cond_signal(&state->cond);
}

void close_button_on(car_shared_mem *state) {
	state->close_button = 1;
	pthread_cond_signal(&state->cond);
}

void stop_car(car_shared_mem *state) {
	state->emergency_stop = 1;
	pthread_cond_signal(&state->cond);
}

void service_on(car_shared_mem *state) {
	state->individual_service_mode = 1;
	if (state->emergency_mode == 1) {
		state->emergency_mode = 0;
		pthread_cond_broadcast(&state->cond);
	} else {
		pthread_cond_signal(&state->cond);
	}
}

void service_off(car_shared_mem *state) {
	state->individual_service_mode = 0;
	pthread_cond_signal(&state->cond);
}

int up(car_shared_mem *state) {
	int result = increment_floor(state->current_floor, state->destination_floor);;
	pthread_cond_signal(&state->cond);
	return result;
}

int down(car_shared_mem *state) {
	int result = decrement_floor(state->current_floor, state->destination_floor);;
	pthread_cond_signal(&state->cond);
	return result;
}

int decrement_floor(char *current_floor, char *destination_floor) {
	int floor_number;

	if (current_floor[0] == 'B') {
		floor_number = atoi(current_floor + 1);
		if (floor_number == 99) {
			return I_MIN_FLOOR_ERROR;
		} else {
			sprintf(destination_floor, "B%d", floor_number + 1);
		}
	} else {
		floor_number = atoi(current_floor);
		if (floor_number == 1) {
			strcpy(destination_floor, "B1");
		} else {
			sprintf(destination_floor, "%d", floor_number - 1);
		}
	}
	return 0;
}

int increment_floor(char *current_floor, char *destination_floor) {
	int floor_number;
	if (current_floor[0] == 'B') {
		floor_number = atoi(current_floor + 1);
		if (floor_number == 1) {
			strcpy(destination_floor, "1");
		} else {
			sprintf(destination_floor, "B%d", floor_number - 1);
		}
	} else {
		floor_number = atoi(current_floor);
		if (floor_number == 999) {
			return I_MAX_FLOOR_ERROR;
		} else {
			sprintf(destination_floor, "%d", floor_number + 1);
		}
	}
	return 0;
}
