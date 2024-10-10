#include <fcntl.h>
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

int main(int argc, char *argv[]) {
	// Check if the correct number of command line arguments was given
	if (argc != 5) {
        fprintf(stderr, "Error: Incorrect number of command line arguments. Expected 4, got %d.\n", argc - 1);
		return 1;
	}

	car_t car;
	car_init(&car, argv[1], argv[2], argv[3], argv[4]);

	pid_t car_thread = start_car(&car);

	wait(&car_thread);

	return 0;
}

pid_t start_car(car_t *car) {
	pid_t p;
	p = fork();
	if (p == 0) {
		pthread_mutex_lock(&car->state->mutex);
		pthread_cond_wait(&car->state->cond, &car->state->mutex);
		pthread_mutex_unlock(&car->state->mutex);

		if (open_button_is(car->state, 1)) {
			set_open_button(car->state, 0);
			do {
				set_status(car->state, next_in_open_cycle(car->state));
				if (usleep_cond(car) == 0) break;
			} while (!status_is(car->state, "Closed"));
		}
		if (close_button_is(car->state, 1)) {
			set_close_button(car->state, 0);
			while (!status_is(car->state, "Closed")) {
				set_status(car->state, next_in_open_cycle(car->state));
				if (usleep_cond(car) == 0) break;
			}
		}
	}
	return p;
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

void cycle_open(car_t *car) {
	set_open_button(car->state, 0);

	const char *statuses[4] = {"Opening", "Open", "Closing", "Closed"};
	for (size_t i = 0; i < 4; i++) {
		set_status(car->state, statuses[i]);
		usleep(car->delay);
	}
}

void close_doors(car_t *car) {
	pthread_mutex_lock(&car->state->mutex);
	// car->state->close_button = 0;
	strcpy(car->state->status, "Closing");
	pthread_cond_broadcast(&car->state->cond);
	pthread_mutex_unlock(&car->state->mutex);

	usleep(car->delay);
	set_status(car->state, "Closed");
}

const char *next_in_open_cycle(car_shared_mem *state) {
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
