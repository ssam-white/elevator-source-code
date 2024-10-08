#pragma once

#include "globals.h"

typedef enum {
	I_SUCCESS = 0,
	I_CONNECTION_ERROR = -1,
	I_SERVICE_MODE_ERROR = -2,
	I_DOORS_ERROR = -3,
	I_BETWEEN_FLOORS_ERROR = -4,
} icontroller_error_t;

typedef struct {
	const char *car_name;
	const char *operation;
	const char *shm_name;
	int fd;
	car_shared_mem *state;
} icontroller_t;

void icontroller_init(icontroller_t *, const char *, const char *);
void icontroller_deinit(icontroller_t *);
int icontroller_connect(icontroller_t *);
bool opIs(icontroller_t *, const char *);
bool statusIs(icontroller_t *, const char *);
void printState(car_shared_mem *);
int decrementFloor(char *);
int incrementFloor(char *);
int carCanMove(icontroller_t *);

int handleOperation(icontroller_t *);

// operations
void open_doors(car_shared_mem *);
void close_doors(car_shared_mem *);
void stop_car(car_shared_mem *);
void service_on(car_shared_mem *);
int up(icontroller_t *);
int down(icontroller_t *);

