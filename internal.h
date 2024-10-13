#pragma once

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
void print_state(car_shared_mem *);
int can_car_move(car_shared_mem *);

int handle_operation(icontroller_t *);

int up(car_shared_mem *);
int down(car_shared_mem *);
bool op_is(icontroller_t *, const char *);

