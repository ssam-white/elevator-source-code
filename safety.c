#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * This is the implementation for the safety component. It connects to a car
 * over POSIX shared memory and ensures that if anything goes wrong with the
 * car's operation, it ends up in emergency mode.
 *
 * This component is safety-critical and has the following safety limitations:
 *
 *  1. Double pointer dereferencing
 *
 *      Double pointer dereferencing is discouraged in MISRA C, but it is
 *      necessary for the system to maintain references to the shared
 *      memory object.
 *
 *  2. Error handling
 *
 *      The error handling for data consistency errors is well-structured,
 *      but the error handling in cases of issues like write() errors or
 *      mutex-related errors could be improved by implementing an error
 *      handling mechanism.
 *
 *      For example, if writes or mutex functions fail, they could be retried
 *      a certain number of times. If they still fail, the car could be put into
 *      emergency mode, as this likely indicates a larger issue.
 *
 *      Additionally, checking of return values is not as rigorous as it should
 *      be according to MISRA C.
 *
 *  3. Use of signal.h
 *
 *      MISRA C discourages the use of signal.h, but it was necessary for this
 *      implementation as I didn’t have a robust way to safely cancel the
 *		program. With more time, I would have found a better solution,
 *		but this was the best I could do for now.
 *
 * I'm Sure there are things I missed here but overall the safety component is
 * quite functional and handles any major errors like shared memory data
 * inconsistancies well. I would have liked to dedicate more time to this
 *component but i ran out of time :(
 */

#include "global.h"
#include "posix.h"
#include "safety.h"

/* Flag to control the main loop, modified by signal handler */
static volatile sig_atomic_t keep_running = 1;

/*
 * Signal handler function for handling SIGINT signal
 */
static void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        keep_running = 0;
    }
}

int main(int argc, char *argv[])
{
    /* Validate correct number of command line args. */
    if (argc != 2)
    {
        write(STDOUT_FILENO, "Incorrect number of command line arguments\n",
              43);
        return 1;
    }

    /* Setup the signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        return 1;
    }

    /* Ignore SIGPIPE to prevent program termination if a message fails to send
     */
    signal(SIGPIPE, SIG_IGN);

    /* Initialize the Safety structure */
    safety_t safety;
    safety_init(&safety, argv[1]);

    /* Connect to the car shared memory object. */
    if (!connect_to_car(&safety.state, safety.shm_name, &safety.fd))
    {
        char buf[50];
        int len = snprintf(buf, sizeof(buf), "Unable to access car %s.\n",
                           safety.car_name);
        write(STDOUT_FILENO, buf, len);
        return 1;
    }

    /* This timestamp will be used when periodically checking the keep_running
     *flag. */
    struct timespec ts;

    /* Loop untill the SIGINT signal is sent */
    while (keep_running)
    {
        /* Set a 1-second timeout */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        /* Acquire the mutex and wait on the condition variable periodically
         * checking if the keep_running flag is set */
        if (pthread_mutex_lock(&safety.state->mutex) != 0)
        {
            return 1;
        }

        int wait_result = pthread_cond_timedwait(&safety.state->cond,
                                                 &safety.state->mutex, &ts);
        if (wait_result != 0 && wait_result != ETIMEDOUT)
        {
            /* If pthread_condwait was interupted then release the mutex and
             * break out of the main loop */
            if (wait_result == EINTR)
            {
                pthread_mutex_unlock(&safety.state->mutex);
                continue;
            }
            perror("pthread_cond_timedwait");
            pthread_mutex_unlock(&safety.state->mutex);
            break;
        }

        /* Check for data consistancy errors */
        if (!is_shm_data_valid(safety.state))
        {
            write(STDOUT_FILENO, "Data consistency error!\n", 24);
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        /* If there is a door obstruction, set the doors to 'Opening' */
        if (strcmp(safety.state->status, "Closing") == 0 &&
            safety.state->door_obstruction == 1)
        {
            strcpy(safety.state->status, "Opening");
            pthread_cond_broadcast(&safety.state->cond);
        }

        /* If the emergency stop button is pressed and a message hasn't been
         * printed about it. Print the message and put the car into emergency
         * mode. */
        if (safety.state->emergency_stop == 1 && safety.emergency_msg_sent == 0)
        {
            write(STDOUT_FILENO,
                  "The emergency stop button has been pressed!\n", 44);
            safety.emergency_msg_sent = 1;
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        /* If the overload sensor has been tripped and a message hasn't been
         * printed, put the car into emergency mode and print the message */
        if (safety.state->overload == 1 && safety.overload_msg_sent == 0)
        {
            write(STDOUT_FILENO, "The overload sensor has been tripped!\n", 38);
            safety.overload_msg_sent = 1;
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        pthread_mutex_unlock(&safety.state->mutex);
    }

    safety_deinit(&safety);

    return 0;
}

/*
 * Initialises the safety data structure with default values.
 */
void safety_init(safety_t *safety, char *car_name)
{
    safety->car_name = car_name;
    safety->shm_name = get_shm_name(car_name);
    safety->fd = -1;
    safety->state = NULL;
    safety->emergency_msg_sent = 0;
    safety->overload_msg_sent = 0;
}

/*
 */
/*
 */
/*
 */
/*
 */
/*
 */
/*
 */
/*
 */
/*
 */
/*
 * Deinitialises the safety object by closing the shared memory object and
 * freeing shm_name.
 */
void safety_deinit(safety_t *safety)
{
    if (safety->state != NULL)
    {
        /* Unmap the shared memory */
        munmap(safety->state, sizeof(car_shared_mem));
        safety->state = NULL;
    }

    if (safety->fd != -1)
    {
        /* Close the file descriptor */
        close(safety->fd);
        safety->fd = -1;
    }

    if (safety->shm_name != NULL)
    {
        /* Free the dynamically allocated shared memory name */
        free(safety->shm_name);
        safety->shm_name = NULL;
    }
}

/*
 * Checks the shared memory objects uint8_t flags for data consistancy errors.
 */
bool is_shm_int_fields_valid(const car_shared_mem *state)
{
    /* Check to see if all uint8_t fields of the shared memory object are either
     * 0 or 1. */
    return (state->open_button < 2 && state->close_button < 2 &&
            state->emergency_mode < 2 && state->emergency_stop < 2 &&
            state->overload < 2 && state->individual_service_mode < 2 &&
            state->door_obstruction < 2);
}

/*
 * Checks if the status in the car shared memory object is 1 of either "Closed",
 * "Opening", "Open", "Closing", or "Between".
 */
bool is_shm_status_valid(const car_shared_mem *state)
{
    /* Define all posible valid states. */
    const char *statuses[] = {"Opening", "Open", "Closing", "Closed",
                              "Between"};
    /* Loop over the valid statuses setting is_valid to true if a match is
     * found. If no match is found then the given status is invalid.  */
    bool is_valid = false;
    for (int i = 0; i < 5; i++)
    {
        if (strcmp(state->status, statuses[i]) == 0)
        {
            is_valid = true;
            break;
        }
    }
    return is_valid;
}

/*
 * Checks if the door obstruction flag in shared memory is set while doors are
 * in a complementary state (i.e. "Opening", "Closing"). If the obstruction flag
 * isn't set then the doors can be in any other valid state.
 */
bool is_shm_obstruction_valid(const car_shared_mem *state)
{
    /* Check to see if the doors are either "Opening" or "closing". */
    bool is_valid_obstruction_state = strcmp(state->status, "Closing") == 0 ||
                                      strcmp(state->status, "Opening") == 0;
    /* If there's no door ubstruction then the obstruction state is fine but if
     * there is the doors must be in 1 of the 2 valid status. */
    if (state->door_obstruction == 0)
        return true;
    else
        return is_valid_obstruction_state;
}

/*
 * Checks the shared memory object for data consistancy errors by checking that
 * all the following conditions true:
 *	- The status is valid.
 *	- The current floor is valid.
 *	- The destination floor is valid.
 *	- All the uint8_t flags are either 0 or 1.
 *	- If there is a door obstruction the status is either "Opening" or
 *	  "Closing".
 */
bool is_shm_data_valid(const car_shared_mem *state)
{
    return (is_shm_status_valid(state) &&
            is_valid_floor(state->current_floor) &&
            is_valid_floor(state->destination_floor) &&
            is_shm_int_fields_valid(state) && is_shm_obstruction_valid(state)

    );
}
