#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "global.h"
#include "posix.h"
#include "safety.h"

/* Flag to control the main loop, modified by signal handler */
static volatile sig_atomic_t keep_running = 1;

/* Signal handler function for handling SIGINT signal */
static void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        keep_running = 0;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        write(STDOUT_FILENO, "Incorrect number of command line arguments\n", 43);
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
        perror("sigaction");
        return 1;
    }

    /* Ignore SIGPIPE to prevent program termination if a message fails to send */
    signal(SIGPIPE, SIG_IGN);

    /* Initialize the Safety structure */
    safety_t safety;
    safety_init(&safety, argv[1]);

    if (!connect_to_car(&safety.state, safety.shm_name, &safety.fd))
    {
        char buf[50];
        int len = snprintf(buf, sizeof(buf), "Unable to access car %s.\n", safety.car_name);
        write(STDOUT_FILENO, buf, len);
        return 1;
    }

    struct timespec ts;

    while (keep_running)
    {
        /* Set a 1-second timeout */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        if (pthread_mutex_lock(&safety.state->mutex) != 0)
        {
            perror("pthread_mutex_lock");
            return 1;
        }

        int wait_result = pthread_cond_timedwait(&safety.state->cond, &safety.state->mutex, &ts);

        if (wait_result != 0 && wait_result != ETIMEDOUT)
        {
            if (wait_result == EINTR) // Retry if interrupted by a signal
            {
                pthread_mutex_unlock(&safety.state->mutex);
                continue;
            }
            perror("pthread_cond_timedwait");
            pthread_mutex_unlock(&safety.state->mutex);
            break;
        }

        if (!is_shm_data_valid(safety.state))
        {
            write(STDOUT_FILENO, "Data consistency error!\n", 24);
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        if (strcmp(safety.state->status, "Closing") == 0 && safety.state->door_obstruction == 1)
        {
            strcpy(safety.state->status, "Opening");
            pthread_cond_broadcast(&safety.state->cond);
        }

        if (safety.state->emergency_stop == 1 && safety.emergency_msg_sent == 0)
        {
            write(STDOUT_FILENO, "The emergency stop button has been pressed!\n", 44);
            safety.emergency_msg_sent = 1;
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        if (safety.state->overload == 1 && safety.overload_msg_sent == 0)
        {
            write(STDOUT_FILENO, "The overload sensor has been tripped!\n", 38);
            safety.overload_msg_sent = 1;
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        pthread_mutex_unlock(&safety.state->mutex);
    }

    return 0;
}

void safety_init(safety_t *safety, char *car_name)
{
    safety->car_name = car_name;
    safety->shm_name = get_shm_name(car_name);
    safety->fd = -1;
    safety->state = NULL;
    safety->emergency_msg_sent = 0;
    safety->overload_msg_sent = 0;
}

bool is_shm_int_fields_valid(const car_shared_mem *state)
{
    return (state->open_button < 2 && state->close_button < 2 &&
            state->emergency_mode < 2 && state->emergency_stop < 2 &&
            state->overload < 2 && state->individual_service_mode < 2 &&
            state->door_obstruction < 2);
}

bool is_shm_status_valid(const car_shared_mem *state)
{
    const char *statuses[] = {"Opening", "Open", "Closing", "Closed",
                              "Between"};
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

bool is_shm_obstruction_valid(const car_shared_mem *state)
{
    // if status is not "Closing" and obstruction is 1 then the obstruction
    // state is invalid
    bool is_valid_obstruction_state = strcmp(state->status, "Closing") == 0 ||
                                      strcmp(state->status, "Opening") == 0;
    if (state->door_obstruction == 0)
        return true;
    else
        return is_valid_obstruction_state;
}

bool is_shm_data_valid(const car_shared_mem *state)
{
    return (is_shm_status_valid(state) &&
            is_valid_floor(state->current_floor) &&
            is_valid_floor(state->destination_floor) &&
            is_shm_int_fields_valid(state) && is_shm_obstruction_valid(state)

    );
}

