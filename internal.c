#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "global.h"
#include "internal.h"
#include "posix.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Incorrect number of command line args.\n");
        return 1;
    }

    // initialize a new internal controller
    icontroller_t icontroller;
    icontroller_init(&icontroller, argv[1], argv[2]);

    // attempt to connect to the car shared emory object
    if (!connect_to_car(&icontroller.state, icontroller.shm_name, &icontroller.fd))
    {
        printf("Unable to access car %s.\n", icontroller.car_name);
        return 1;
    }

    switch (handle_operation(&icontroller))
    {
    case I_SERVICE_MODE_ERROR:
        printf("Operation only allowed in service mode.\n");
        break;
    case I_DOORS_ERROR:
        printf("Operation not allowed while doors are open.\n");
        break;
    case I_BETWEEN_FLOORS_ERROR:
        printf("Operation not allowed while elevator is moving.\n");
        break;
    case I_INVALID_OPERATION:
        printf("Invalid operation.\n");
        break;
    default:
        break;
    }

    icontroller_deinit(&icontroller);

    return 0;
}

void icontroller_init(icontroller_t *icontroller, const char *car_name, const char *operation)
{
    icontroller->car_name = car_name;
    icontroller->operation = operation;
    icontroller->shm_name = get_shm_name(car_name);
    icontroller->fd = -1;
}

void icontroller_deinit(icontroller_t *icontroller)
{
    icontroller->car_name = NULL;
    icontroller->operation = NULL;

    if (icontroller->fd >= 0)
    {
        close(icontroller->fd);
        icontroller->fd = -1;
    }

    if (icontroller->shm_name != NULL)
    {
        free(icontroller->shm_name);
        icontroller->shm_name = NULL;
    }

    if (icontroller->state != NULL)
    {
        munmap(icontroller->state, sizeof(car_shared_mem));
        icontroller->state = NULL;
    }
}

int can_car_move(car_shared_mem *state)
{
    pthread_mutex_lock(&state->mutex);
    bool is_between = strcmp(state->status, "Between") == 0;
    bool is_closed = strcmp(state->status, "Closed");
    pthread_mutex_unlock(&state->mutex);

    if (service_mode_is(state, 0))
    {
        return I_SERVICE_MODE_ERROR;
    }
    else if (is_between)
    {
        return I_BETWEEN_FLOORS_ERROR;
    }
    else if (is_closed)
    {
        return I_DOORS_ERROR;
    }
    return I_SUCCESS;
}

int handle_operation(icontroller_t *icontroller)
{
    car_shared_mem *state = icontroller->state;

    if (status_is(state, "Open"))
    {
        set_open_button(state, 1);
    }
    else if (op_is(icontroller, "close"))
    {
        set_close_button(state, 1);
    }
    else if (op_is(icontroller, "stop"))
    {
        set_emergency_stop(state, 1);
    }
    else if (op_is(icontroller, "service_on"))
    {
        set_service_mode(state, 1);
    }
    else if (op_is(icontroller, "service_off"))
    {
        set_service_mode(state, 0);
    }
    else if (op_is(icontroller, "up") || op_is(icontroller, "down"))
    {
        int can_move = can_car_move(state);
        if (can_move < 0)
            return can_move;

        return op_is(icontroller, "up") ? up(state) : down(state);
    }
    else
    {
        return I_INVALID_OPERATION;
    }
    return I_SUCCESS;
}

int up(car_shared_mem *state)
{
    pthread_mutex_lock(&state->mutex);
    int result = increment_floor(state->destination_floor);
    if (result == I_SUCCESS)
    {
        pthread_cond_broadcast(&state->cond);
    }
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int down(car_shared_mem *state)
{
    pthread_mutex_lock(&state->mutex);
    int result = decrement_floor(state->destination_floor);
    if (result == I_SUCCESS)
    {
        pthread_cond_broadcast(&state->cond);
    }
    pthread_mutex_unlock(&state->mutex);
    return result;
}

bool op_is(const icontroller_t *icontroller, const char *op)
{
    return strcmp(icontroller->operation, op) == 0;
}
