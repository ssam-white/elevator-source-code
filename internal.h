#pragma once

#include "global.h"

typedef enum {
	I_SUCCESS = 0,
	I_SERVICE_MODE_ERROR = -1,
	I_DOORS_ERROR = -2,
	I_BETWEEN_FLOORS_ERROR = -3,
	I_INVALID_OPERATION = -4,
	I_MAX_FLOOR_ERROR = -5,
	I_MIN_FLOOR_ERROR = -6,
} icontroller_error_t;

typedef struct {
	const char *car_name;
	const char *operation;
	char *shm_name;
	int fd;
	car_shared_mem *state;
} icontroller_t;

void icontroller_init(icontroller_t *, const char *, const char *);
void icontroller_deinit(icontroller_t *);
bool icontroller_connect(icontroller_t *);
bool op_is(icontroller_t *, const char *);
bool status_is(car_shared_mem *, const char *);
void print_state(car_shared_mem *);
int decrement_floor(char *, char *);
int increment_floor(char *, char *);
int can_car_move(car_shared_mem *);

int handle_operation(icontroller_t *);

// operations
void open_button_on(car_shared_mem *);
void close_button_on(car_shared_mem *);
void stop_car(car_shared_mem *);
void service_on(car_shared_mem *);
void service_off(car_shared_mem *);
int up(car_shared_mem *);
int down(car_shared_mem *);

