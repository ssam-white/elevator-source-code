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

#include "car.h"

int main(int argc, char *argv[]) {
	// Check if the correct number of command line arguments was given
	if (argc != 5) {
        fprintf(stderr, "Error: Incorrect number of command line arguments. Expected 4, got %d.\n", argc - 1);
		return 1;
	}

	car_t car;
	car_init(&car, argv[1], argv[2], argv[3], argv[4]);

	print_car(&car);

	return 0;
}

void car_init(car_t* car, char* name, char* lowest_floor, char* highest_floor, char* delay) {
	car->name = name;
	car->lowest_floor = lowest_floor;
	car->highest_floor = highest_floor;
	car->delay = delay;

	// create the shared memory object for the cars state
	if (!create_shared_object(car, car->name)) {
		perror("Failed to create shared object");
		exit(1);
	}
}

bool create_shared_object( car_t* car, const char* name ) {
    // Remove any previous instance of the shared memory object, if it exists.
	shm_unlink(name);

    // Assign car name to car->name.
	car->name = name;

    // Create the shared memory object, allowing read-write access by all users,
    // and saving the resulting file descriptor in car->fd. If creation failed,
    // ensure that car->state is NULL and return false.
	car->fd = shm_open(car->name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
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

void print_car(car_t* car) {
	printf("car_t: { name: %s, lowest_floor: %s, highest_floor: %s, delay: %s }\n",
		car->name,
		car->lowest_floor,
		car->highest_floor,
		car->delay
	);
}

