#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <string.h>
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
#include "car.h"

volatile sig_atomic_t shutdown_requested = 0;

int main(int argc, char *argv[]) {
	// Check if the correct number of command line arguments was given
	if (argc != 5) {
        fprintf(stderr, "Error: Incorrect number of command line arguments. Expected 4, got %d.\n", argc - 1);
		return 1;
	}

	signal(SIGINT, handle_sigint);



	car_t car;
	car_init(&car, argv[1], argv[2], argv[3], argv[4]);


	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_create(&car.door_thread, &attr, handle_doors, &car);
	pthread_create(&car.level_thread, &attr, handle_level, &car);

	while (!shutdown_requested);

	pthread_cancel(car.door_thread);
	pthread_cancel(car.level_thread);

	pthread_join(car.door_thread, NULL);
	pthread_join(car.level_thread, NULL);

	car_deinit(&car);

	return 0;
}

void *handle_doors(void *arg) {
	car_t *car = (car_t *) arg;

	while (1) {
		pthread_mutex_lock(&car->state->mutex);
		pthread_cond_wait(&car->state->cond, &car->state->mutex);
		pthread_mutex_unlock(&car->state->mutex);

		if (open_button_is(car->state, 1)) {
			set_open_button(car->state, 0);
			open_doors(car);
			if (service_mode_is(car->state, 0)) {
				close_doors(car);
			}
		}


		if (close_button_is(car->state, 1)) {
			set_close_button(car->state, 0);
			close_doors(car);
		}

		pthread_testcancel();
	}

}

void *handle_level(void *arg) {
	car_t *car = (car_t *) arg;

	while (1) {
		pthread_mutex_lock(&car->state->mutex);
		pthread_cond_wait(&car->state->cond, &car->state->mutex);
		pthread_mutex_unlock(&car->state->mutex);

		if (new_destination(car->state) && check_destination(car) == 0) {
			set_status(car->state, "Between");
			usleep(car->delay);

			pthread_mutex_lock(&car->state->mutex);
			strcpy(car->state->status, "Closed");
			strcpy(car->state->current_floor, car->state->destination_floor);
			pthread_cond_broadcast(&car->state->cond);
			pthread_mutex_unlock(&car->state->mutex);
		} else {
			set_string(car->state, car->state->destination_floor, car->state->current_floor);
		}
		pthread_testcancel();
	}
}

void car_init(car_t* car, char* name, char* lowest_floor, char* highest_floor, char* delay) {
	car->name = name;
	car->shm_name = get_shm_name(car->name);
	car->lowest_floor = lowest_floor;
	car->highest_floor = highest_floor;
	car->delay = (uint8_t) atoi(delay) * 1000;

	// create the shared memory object for the cars state
	if (!create_shared_mem(car, car->name)) {
		perror("Failed to create shared object");
		exit(1);
	}

	init_shm(car->state);
	strcpy(car->state->current_floor, car->lowest_floor);
	strcpy(car->state->destination_floor, car->lowest_floor);
}

void car_deinit(car_t *car) {
	munmap(car->state, sizeof(car_shared_mem));
	shm_unlink(car->shm_name);
	car->state = NULL;

	car->name = NULL;
	free(car->shm_name);
	car->highest_floor = NULL;
	car->lowest_floor = NULL;
	car->delay = 0;	
}

bool create_shared_mem( car_t* car, char* name ) {
    // Remove any previous instance of the shared memory object, if it exists.
	shm_unlink(name);

    // Assign car name to car->name.
	car->name = name;

    // Create the shared memory object, allowing read-write access by all users,
    // and saving the resulting file descriptor in car->fd. If creation failed,
    // ensure that car->state is NULL and return false.
	car->fd = shm_open(car->shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (car->fd == -1) {
		car->state = NULL;
		return false;
	}
	

    // Set the capacity of the shared memory object via ftruncate. If the 
    // operation fails, ensure that car->state is NULL and return false. 
	if (ftruncate(car->fd, sizeof(car_shared_mem))) {
		car->state = NULL;
		return false;
	}

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in car->state. If mapping fails, return false.
	car->state = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, car->fd, 0);
	if (car->state == MAP_FAILED) {
		return false;
	}

    return true;
}

const char *next_in_cycle(car_shared_mem *state) {
	if (status_is(state, "Closed")) {
		return "Opening";
	} else if (status_is(state, "Opening")) {
		return "Open";
	} else if (status_is(state, "Open")) {
		return "Closing";
	} else if(status_is(state, "Closing")) {
		return "Closed";
	}
	return NULL;
}

void open_doors(car_t *car) {
	if (status_is(car->state, "Open")) usleep(car->delay);
	do {
		set_status(car->state, next_in_cycle(car->state));
		usleep(car->delay);
	} while (!status_is(car->state, "Open"));
}

void close_doors(car_t *car) {
	// if (status_is(car->state, "Opening")) set_status(car->state, "Closing");
	while (!status_is(car->state, "Closed")) {
		set_status(car->state, next_in_cycle(car->state));
		usleep(car->delay);
	}
}

const char *next_in_close_cycle(car_shared_mem *state) {
	if (status_is(state, "Open")) {
		return "Closing";
	} else if (status_is(state, "Closing")) {
		return "Closed";
	}
	return NULL;
}

int usleep_cond(car_t *car) {
	pthread_mutex_lock(&car->state->mutex);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);  // Get the current time
	ts.tv_nsec += car->delay * 1000;

	int result = pthread_cond_timedwait(&car->state->cond, &car->state->mutex, &ts);

	pthread_mutex_unlock(&car->state->mutex);

	return result;
}

void handle_sigint(int sig) {
	switch (sig) {
		case SIGINT:
			shutdown_requested = 1;
			break;
		default:
			break;
	}
}

int floor_to_int(char *floor) {
	int floor_number;
	char temp_floor[4];
	strcpy(temp_floor, floor);
	if (temp_floor[0] == 'B') {
		temp_floor[0] = '-';
		floor_number = atoi(temp_floor);
	} else {
		floor_number = atoi(temp_floor) - 1;
	}
	return floor_number;
}

int check_destination (car_t *car) {
	pthread_mutex_lock(&car->state->mutex);
	int floor_number = floor_to_int(car->state->destination_floor);
	int lowest_floor_number = floor_to_int(car->lowest_floor);
	int highest_floor_number = floor_to_int(car->highest_floor);
	pthread_mutex_unlock(&car->state->mutex);

	if (floor_number < lowest_floor_number) {
		return -1;
	} else if (floor_number > highest_floor_number) {
		return 1;
	} else {
		return 0;
	}
}

bool new_destination(car_shared_mem *state) {
	pthread_mutex_lock(&state->mutex);
	bool result = strcmp(state->destination_floor, state->current_floor);
	pthread_mutex_unlock(&state->mutex);
	return result;
}
