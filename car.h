#pragma once

#include <pthread.h>
#include <stdint.h>

#include "posix.h"

typedef struct {
	char *name;
	char *shm_name;
	char *lowest_floor;
	char *highest_floor;
	uint32_t delay;
	int fd;
	car_shared_mem *state;
} car_t;

void car_init(car_t*, char*, char*, char*, char*);
pid_t start_car(car_t *);
bool create_shared_mem( car_t *, char *);
void print_car(car_t*);

void cycle_open(car_t *);
void close_doors(car_t *);

const char *next_in_open_cycle(car_shared_mem *);
const char *next_in_close_cycle(car_shared_mem *);

int usleep_cond(car_t *);
