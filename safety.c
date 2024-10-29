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

#include "global.h"
#include "posix.h"
#include "safety.h"

int main(int argc, char *argv[]) 
{
	if (argc != 2) {
		return 1;
	}

	safety_t safety;
	safety_init(&safety, argv[1]);

	if (!safety_connect(&safety)) {
		printf("Unable to access car %s.\n", safety.car_name);
		return 1;
	}

	while (1) {
		pthread_mutex_lock(&safety.state->mutex);
		pthread_cond_wait(&safety.state->cond, &safety.state->mutex);


		if (
			!is_valid_floor(safety.state->current_floor) ||
			!is_valid_floor(safety.state->destination_floor) ||
			safety.state->open_button > 1 ||
			safety.state->close_button > 1 ||
			safety.state->emergency_mode > 1 ||
			safety.state->emergency_stop > 1 ||
			safety.state->overload > 1 ||
			safety.state->individual_service_mode > 1 ||
			safety.state->door_obstruction > 1 ||
			( 
				(
					strcmp(safety.state->status, "Open") == 0 ||
					strcmp(safety.state->status, "Between") == 0 ||
					strcmp(safety.state->status, "Closed") == 0
				) 
				&& safety.state->door_obstruction == 1 
			)
		) {
			printf("Data consistancy error!\n");
			safety.state->emergency_mode = 1;
			pthread_cond_broadcast(&safety.state->cond);
		}

		if (strcmp(safety.state->status, "Closing") == 0 && safety.state->door_obstruction == 1) {
			strcpy(safety.state->status, "Opening");
			pthread_cond_broadcast(&safety.state->cond);
		} else if (strcmp(safety.state->status, "Closed") == 0) {
		} else if (strcmp(safety.state->status, "Opening") == 0) {
		} else if (strcmp(safety.state->status, "Open") == 0) {
		} else if (strcmp(safety.state->status, "Between") == 0) {
		} else {
			printf("Data consistancy error.\n");
			safety.state->emergency_mode = 1;
			pthread_cond_broadcast(&safety.state->cond);
		}

		if (safety.state->emergency_stop == 1) {
			if (safety.emergency_msg_sent == 0) {
				printf("The emergency stop button has been pressed!\n");
				safety.emergency_msg_sent = 1;
			}
			safety.state->emergency_mode = 1;
			pthread_cond_broadcast(&safety.state->cond);
		}
		if (safety.state->overload == 1) {
			if (safety.overload_msg_sent == 0) {
				printf("The overload sensor has been tripped!\n");
				safety.overload_msg_sent = 1;
			}
			safety.state->emergency_mode = 1;
			pthread_cond_broadcast(&safety.state->cond);
		}
		pthread_mutex_unlock(&safety.state->mutex);


	}

	return 0;
}

void safety_init(safety_t *safety, char *car_name)
{
	safety->car_name = car_name;
	safety->shm_name = get_shm_name(car_name);
	safety->fd = -1;
	safety->state = NULL;
	safety->emergency_msg_sent = 0;
	safety->overload_msg_sent = 0;
}

bool safety_connect(safety_t *safety)
{
	safety->fd = shm_open(safety->shm_name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (safety->fd < 0) {
		return false;
	}

	 safety->state = mmap(0, sizeof(*safety->state), PROT_READ | PROT_WRITE, MAP_SHARED, safety->fd, 0);
	if (safety->state == NULL) {
		return false;
	}

	return true;
}
