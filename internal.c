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

/*
 * This is the implementation of the internal controller. The controls interact
 * with the shared memory segment to indicate passenger requests, such as
 * opening or closing the doors. It also manages elevator operations in service
 * mode.
 */

#include "global.h"
#include "internal.h"
#include "posix.h"

int main(int argc, char *argv[])
{
    /* Check if the correct number of command line arguments is provided */
    if (argc != 3)
    {
        printf("Incorrect number of command line args.\n");
        return 1;
    }

    /* Declare and initialise the elevator controller */
    icontroller_t icontroller;
    icontroller_init(&icontroller, argv[1], argv[2]);

    /* Attempt to connect to the car's shared memory object */
    if (!connect_to_car(&icontroller.state, icontroller.shm_name,
                        &icontroller.fd))
    {
        printf("Unable to access car %s.\n", icontroller.car_name);
        icontroller_deinit(&icontroller);
        return 1;
    }

    /*
     * Handle the specified operation and print any relevant error messages. All
     * messages printed as the program’s output are handled here.
     */
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
    case I_INVALID_OPERATION_ERROR:
        printf("Invalid operation.\n");
        break;
    default:
        break;
    }

    /* Clean up and release resources */
    icontroller_deinit(&icontroller);

    return 0;
}

/*
 * Initialise the elevator controller with car name and operation
 */
void icontroller_init(icontroller_t *icontroller, const char *car_name,
                      const char *operation)
{
    icontroller->car_name = car_name;
    icontroller->operation = operation;
    icontroller->shm_name = get_shm_name(car_name);
    icontroller->fd = -1;
}

/*
 * Deinitialise the elevator controller and release allocated resources
 */
void icontroller_deinit(icontroller_t *icontroller)
{
    icontroller->car_name = NULL;
    icontroller->operation = NULL;

    /* Close file descriptor if it's open */
    if (icontroller->fd >= 0)
    {
        close(icontroller->fd);
        icontroller->fd = -1;
    }

    /* Free the shared memory name string */
    if (icontroller->shm_name != NULL)
    {
        free(icontroller->shm_name);
        icontroller->shm_name = NULL;
    }

    /* Unmap shared memory if it's mapped */
    if (icontroller->state != NULL)
    {
        munmap(icontroller->state, sizeof(car_shared_mem));
        icontroller->state = NULL;
    }
}

/*
 * Check if the car is in a state that allows it to move
 */
int can_car_move(car_shared_mem *state)
{
    /* Acquire the mutex to access shared state */
    pthread_mutex_lock(&state->mutex);
    /* Check if car is between floors */
    bool is_between = strcmp(state->status, "Between") == 0;
    /* Check if doors are closed */
    bool is_closed = strcmp(state->status, "Closed") == 0;
    pthread_mutex_unlock(&state->mutex);

    /*
     * The car can't move if it's in individual service mode, if doors aren't
     * closed, or if the car is between floors.
     */
    if (service_mode_is(state, 0))
    {
        return I_SERVICE_MODE_ERROR;
    }
    else if (is_between)
    {
        return I_BETWEEN_FLOORS_ERROR;
    }
    else if (!is_closed)
    {
        return I_DOORS_ERROR;
    }
    return I_SUCCESS;
}

/*
 * Handle the specified operation by setting the corresponding control flags
 * in shared memory. Each operation corresponds to a specific elevator action:
 */
int handle_operation(icontroller_t *icontroller)
{
    car_shared_mem *state = icontroller->state;

    /* Check the requested operation and set the corresponding field in shared
     * memory */
    if (op_is(icontroller, "open"))
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
        /* Check if the car is allowed to move */
        int can_move = can_car_move(state);
        /* If the car can't move, return the response so that the caller knows
         * why */
        if (can_move < 0)
            return can_move;

        /* Move the car up or down based on the operation and return the
         * response to the caller */
        return op_is(icontroller, "up") ? up(state) : down(state);
    }
    else
    {
        return I_INVALID_OPERATION_ERROR; /* Return error if operation is
                                             invalid */
    }
    return I_SUCCESS;
}

/*
 * Move the elevator car up by incrementing the destination floor.
 * Broadcasts a signal to other threads waiting on the condition variable
 * so they can respond to the updated floor.
 */
int up(car_shared_mem *state)
{
    pthread_mutex_lock(&state->mutex);
    int result = increment_floor(state->destination_floor);
    /* Only broadcast if the floor was incremented. */
    if (result == 0)
    {
        pthread_cond_broadcast(&state->cond);
    }
    pthread_mutex_unlock(&state->mutex);
    return result;
}

/*
 * Move the elevator car down by decrementing the destination floor.
 * Broadcasts a signal to other threads waiting on the condition variable
 * so they can respond to the updated floor.
 */
int down(car_shared_mem *state)
{
    pthread_mutex_lock(&state->mutex);
    int result = decrement_floor(state->destination_floor);
    /* Only broadcast if the floor was decremented. */
    if (result == 0)
    {
        pthread_cond_broadcast(&state->cond);
    }
    pthread_mutex_unlock(&state->mutex); /* Unlock mutex */
    return result;
}

/*
 * Check if the given operation matches the operation in shared memory.
 */
bool op_is(const icontroller_t *icontroller, const char *op)
{
    return strcmp(icontroller->operation, op) == 0;
}
