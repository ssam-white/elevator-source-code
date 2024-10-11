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
	pthread_t door_thread;
	pthread_t level_thread;
	car_shared_mem *state;
} car_t;

void car_init(car_t*, char*, char*, char*, char*);
void car_deinit(car_t *);
pid_t start_car(car_t *);
void *handle_doors(void *);
void *handle_level(void *);
bool create_shared_mem( car_t *, char *);

void open_doors(car_t *);
void close_doors(car_t *);

const char *next_in_cycle(car_shared_mem *);
const char *next_in_close_cycle(car_shared_mem *);

int usleep_cond(car_t *);
void handle_sigint(int);

int floor_to_int(char *);
int check_destination(car_t *);
bool new_destination(car_shared_mem *);
