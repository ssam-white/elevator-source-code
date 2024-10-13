#include <fcntl.h>
#include <errno.h>  // Include this for ETIMEDOUT
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
#include "tcpip.h"
#include "global.h"
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

	if (connect_to_controller(&car)) {
		car.connected_to_controller = true;
		pthread_create(&car.receiver_thread, &attr, handle_receiver, &car);
		send_message(car.server_fd, "CAR %s %s %s", car.name, car.lowest_floor, car.highest_floor);
		signal_controller(&car);
	}

	while (!shutdown_requested) {
		car_shared_mem state = *car.state;

		// usleep_cond(&car);
		pthread_mutex_lock(&car.state->mutex);
		pthread_cond_wait(&car.state->cond, &car.state->mutex);
		pthread_mutex_unlock(&car.state->mutex);

		if (
			car.connected_to_controller &&
			( strcmp(state.status, car.state->status) != 0 ||
			  strcmp(state.current_floor, car.state->current_floor) != 0 ||
			  strcmp(car.state->destination_floor, car.state->destination_floor) != 0 )
		) {
			signal_controller(&car);
		}
	}

	pthread_cancel(car.door_thread);
	pthread_cancel(car.level_thread);
	if (car.connected_to_controller) pthread_cancel(car.receiver_thread);

	pthread_join(car.door_thread, NULL);
	pthread_join(car.level_thread, NULL);
	if (car.connected_to_controller) pthread_join(car.receiver_thread, NULL);

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

			set_status(car->state, "Opening");
			usleep(car->delay);
			set_status(car->state, "Open");


			if ((usleep_cond(car) == 0) && service_mode_is(car->state, 0)) {
				close_doors(car);
			}
		}

		if (close_button_is(car->state, 1)) {
			close_doors(car);
			set_close_button(car->state, 0);
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

		int result = cdcmp_floors(car->state);

		if (result != 0) {
			pthread_mutex_lock(&car->state->mutex);
			int bounds_check = bounds_check_floor(car, car->state->destination_floor);
			pthread_mutex_unlock(&car->state->mutex);

			if (bounds_check == -1) {
				set_string(car->state, car->state->destination_floor, car->lowest_floor);
				continue;
			} else if(bounds_check == 1) {
				set_string(car->state, car->state->destination_floor, car->highest_floor);
				continue;
			}

			set_status(car->state, "Between");
			while (cdcmp_floors(car->state) != 0) {
				usleep(car->delay);
				if (cdcmp_floors(car->state) > 0) {
					pthread_mutex_lock(&car->state->mutex);
					if (increment_floor(car->state->current_floor) == 0) {
						pthread_cond_broadcast(&car->state->cond);
					}
					pthread_mutex_unlock(&car->state->mutex);
				} else {
					pthread_mutex_lock(&car->state->mutex);
					if (decrement_floor(car->state->current_floor) == 0) {
						pthread_cond_broadcast(&car->state->cond);
					}
					pthread_mutex_unlock(&car->state->mutex);
				}

				if (cdcmp_floors(car->state) == 0) {
					set_status(car->state, "Closed");
				}
			}

			if (service_mode_is(car->state, 0)) {
				set_status(car->state, "Opening");
				usleep(car->delay);
				set_status(car->state, "Open");
				usleep(car->delay);
				set_status(car->state, "Closing");
				usleep(car->delay);
				set_status(car->state, "Closed");
			}
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
	car->connected_to_controller = false;

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
	set_status(car->state, "Opening");
	usleep(car->delay);
	set_status(car->state, "Open");
}

void close_doors(car_t *car) {
	if (status_is(car->state, "Closed")) return;
	set_status(car->state, "Closing");
	// usleep_cond(car);
	usleep(car->delay);
	set_status(car->state, "Closed");
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
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += car->delay * 1000;

	while (true) {
		pthread_mutex_lock(&car->state->mutex);
		int result = pthread_cond_timedwait(&car->state->cond, &car->state->mutex, &ts);
		pthread_mutex_unlock(&car->state->mutex);

		// Check if a door button was pressed
		if (open_button_is(car->state, 1) || close_button_is(car->state, 1)) {
			return -1;  // Interrupt sleep if a door button is pressed
		}

		// If the wait timed out, just return
		if (result == ETIMEDOUT) {
			return 0;
		}
	}
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
	char temp_floor[5];
	strcpy(temp_floor, floor);
	if (temp_floor[0] == 'B') {
		temp_floor[0] = '-';
		floor_number = atoi(temp_floor);
	} else {
		floor_number = atoi(temp_floor) - 1;
	}
	return floor_number;
}

int bounds_check_floor(car_t *car, char *floor) {

	int floor_number = floor_to_int(floor);
	int lowest_floor_number = floor_to_int(car->lowest_floor);
	int highest_floor_number = floor_to_int(car->highest_floor);

	printf("%d %d %d", floor_number, lowest_floor_number, highest_floor_number);

	if (floor_number < lowest_floor_number) {
		return -1;
	} else if (floor_number > highest_floor_number) {
		return 1;
	} else {
		return 0;
	}
}

int cdcmp_floors(car_shared_mem *state) {
	pthread_mutex_lock(&state->mutex);
	int result = strcmp(state->destination_floor, state->current_floor);
	pthread_mutex_unlock(&state->mutex);
	return result;
}

bool connect_to_controller(car_t *car) {
	// create the socket
	if ((car->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return false;
	}

	// set the sockets address
	car->server_addr.sin_family = AF_INET;
	car->server_addr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, URL, &car->server_addr.sin_addr) <= 0) {
		close(car->server_fd);
		return false;
	}

	// connect to the server
	if (connect(car->server_fd, (struct sockaddr *)&car->server_addr,
			 sizeof(car->server_addr)) < 0) {
		close(car->server_fd);
		return false;
	}

	return true;
}

void *handle_receiver(void *arg) {
	car_t *car = (car_t *) arg;

	while (1) {
		char *message = receive_msg(car->server_fd);		

		if (strstr(message, "FLOOR")) {
			if (bounds_check_floor(car, message + 6) == 0) {
				set_string(car->state, car->state->destination_floor, message + 6);
			}
		}

		pthread_testcancel();
	}
}

void signal_controller(car_t *car) {
	send_message(car->server_fd, "STATUS %s %s %s", car->state->status, car->state->current_floor, car->state->destination_floor);
}

