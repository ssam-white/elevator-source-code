#include <string.h>
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

	icontroller_t icontroller;
	icontroller_init(&icontroller, argv[1], argv[2]);

	if (icontroller_connect(&icontroller) < 0) {
		printf("Unable to access car %s.\n", icontroller.car_name);
		return 1;
	}

	switch (handleOperation(&icontroller)) {
		case I_SERVICE_MODE_ERROR:
			printf("Operation only allowed in service mode.\n");
			break;
		case I_DOORS_ERROR:
			printf("Operation not allowed while doors are open.\n");
			break;
		case I_BETWEEN_FLOORS_ERROR:
			printf("Operation not allowed while elevator is moving.\n");
			break;
		case I_SUCCESS:
			printState(icontroller.state);
			break;
		defult:
			break;
	}

	icontroller_deinit(&icontroller);

	return 0;
}

void icontroller_init(icontroller_t *icontroller, const char *car_name, const char *operation) {
	icontroller->car_name = car_name;
	icontroller->operation = operation;

	char shm_name[strlen(icontroller->car_name) + 4];
	sprintf(shm_name, "/car%s", icontroller->car_name);
	icontroller->shm_name = shm_name;

	icontroller->fd = -1;
}

void icontroller_deinit(icontroller_t *icontroller) {
	icontroller->car_name = NULL;
	icontroller->operation = NULL;
	icontroller->fd = -1;
	shm_unlink(icontroller->shm_name);
	icontroller->shm_name = NULL;
	munmap(icontroller->state, sizeof(car_shared_mem));

}

int icontroller_connect(icontroller_t *icontroller) {
	icontroller->fd = shm_open(icontroller->shm_name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (icontroller->fd < 0) {
		return -1;
	}

	 icontroller->state = mmap(0, sizeof(*icontroller->state), PROT_READ | PROT_WRITE, MAP_SHARED, icontroller->fd, 0);

	return 0;
}

bool opIs(icontroller_t *icontroller, const char *op) {
	return strcmp(icontroller->operation, op) == 0;
}

bool statusIs(icontroller_t *icontroller, const char *status) {
	return strcmp(icontroller->state->status, status) == 0;
}

void printState(car_shared_mem *state) {
	printf("Current state: {%s, %s, %s, %d, %d, %d, %d, %d, %d, %d}\n",
		state->current_floor,
		state->destination_floor,
		state->status,
		state->open_button,
		state->close_button,
		state->door_obstruction,
		state->overload,
		state->emergency_stop,
		state->individual_service_mode,
		state->emergency_mode
	);
}

int carCanMove(icontroller_t *icontroller) {
	if (icontroller->state->individual_service_mode == 0) {
		return I_SERVICE_MODE_ERROR;
	} else if (statusIs(icontroller, "Between")) {
		return I_BETWEEN_FLOORS_ERROR;
	} else if(!statusIs(icontroller, "Closed")) {
		return I_DOORS_ERROR;
	} 
	return I_SUCCESS;
}

int handleOperation(icontroller_t *icontroller) {
	car_shared_mem *state = icontroller->state;

	if (opIs(icontroller, "open")) {
		open_doors(state);
	}	 else if(opIs(icontroller, "close")) {
		close_doors(state);
	} else if(opIs(icontroller, "stop")) {
		stop_car(state);
	} else if (opIs(icontroller, "up")) {
		return up(icontroller);
	} else if (opIs(icontroller, "down")) {
		return down(icontroller);
	} else if (opIs(icontroller, "service_on")) {
		service_on(state);
	}
	return I_SUCCESS;
}

void open_doors(car_shared_mem *state) {
	state->open_button = 1;
}

void close_doors(car_shared_mem *state) {
	state->close_button = 1;
}

void stop_car(car_shared_mem *state) {
	state->emergency_stop = 1;
}

void service_on(car_shared_mem *state) {
	state->individual_service_mode = 1;
}

int up(icontroller_t *icontroller) {
	int can_move = carCanMove(icontroller);
	if (can_move < 0) {
		return can_move;
	}
	return incrementFloor(icontroller->state->destination_floor);;
}

int down(icontroller_t *icontroller) {
	int can_move = carCanMove(icontroller);
	if (can_move < 0) {
		return can_move;
	}
	return decrementFloor(icontroller->state->destination_floor);
}

int decrementFloor(char *floor) {
	int floor_number;

	if (floor[0] == 'B') {
		floor_number = atoi(floor + 1);
		if (floor_number == 99) {
			return -3;
		} else {
			sprintf(floor, "B%d", floor_number + 1);
		}
	} else {
		floor_number = atoi(floor);
		if (floor_number == 1) {
			strcpy(floor, "B1");
		} else {
			sprintf(floor, "%d", floor_number - 11);
		}
	}
	return 0;
}

int incrementFloor(char *floor) {
	int floor_number;
	if (floor[0] == 'B') {
		floor_number = atoi(floor + 1);
		if (floor_number == 1) {
			strcpy(floor, "1");
		} else {
			sprintf(floor, "B%d", floor_number);
		}
	} else {
		floor_number = atoi(floor);
		if (floor_number == 999) {
			return -1;
		} else {
			sprintf(floor, "%d", floor_number + 1);
		}
	}
	return 0;
}
