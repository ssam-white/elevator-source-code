#pragma once

/*
 * This header file defines the `icontroller_t` struct for managing the internal
 * controller state, the `icontroller_error_t` enumeration for operation status
 * codes, and function prototypes for elevator operations and shared memory
 * interactions in internal.c.
 *
 * See internal.c for implementation details.
 */

#include "posix.h"

/*
 * Enum representing possible error codes for car controller operations
 */
typedef enum
{
    I_SUCCESS = 0,
    I_SERVICE_MODE_ERROR = -1,
    I_DOORS_ERROR = -2,
    I_BETWEEN_FLOORS_ERROR = -3,
    I_INVALID_OPERATION_ERROR = -4,
} icontroller_error_t;

/*
 * Struct holding data for interacting with an individual car controller
 */
typedef struct icontroller
{
    const char *car_name;
    char *shm_name;
    const char *operation;
    int fd;
    car_shared_mem *state;
} icontroller_t;

void icontroller_init(icontroller_t *, const char *, const char *);
void icontroller_deinit(icontroller_t *);
void print_state(car_shared_mem *);
int can_car_move(car_shared_mem *);

int handle_operation(icontroller_t *);

int up(car_shared_mem *);
int down(car_shared_mem *);
bool op_is(const icontroller_t *, const char *);
