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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        int result = write(1, "Incorrect number of command line arguments\n", 43);
        if (result < 0)
            return 1;
        return 1;
    }

    safety_t safety;
    safety_init(&safety, argv[1]);

    if (!connect_to_car(&safety.state, safety.shm_name, &safety.fd))
    {
        char buf[50];
        int len = snprintf(buf, sizeof(buf), "Unable to access car %s.\n", safety.car_name);
        int _ = write(1, buf, len);
        return 1;
    }

    while (1)
    {
        if (pthread_mutex_lock(&safety.state->mutex) < 0)
        {
            return 1;
        }

        if (pthread_cond_wait(&safety.state->cond, &safety.state->mutex) < 0)
        {
            return 1;
        }

        if (!is_shm_data_valid(safety.state))
        {
            int result = write(1, "Data consistancy error!\n", 24);
            if (result < 0)
                return 1;

            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        if (strcmp(safety.state->status, "Closing") == 0 && safety.state->door_obstruction == 1)
        {
            strcpy(safety.state->status, "Opening");
            pthread_cond_broadcast(&safety.state->cond);
        }

        if (safety.state->emergency_stop == 1)
        {
            if (safety.emergency_msg_sent == 0)
            {
                int result = write(1, "The emergency stop button has been pressed!\n", 44);
                if (result < 0)
                    return 1;
                safety.emergency_msg_sent = 1;
            }
            safety.state->emergency_mode = 1;
            pthread_cond_broadcast(&safety.state->cond);
        }

        if (safety.state->overload == 1)
        {
            if (safety.overload_msg_sent == 0)
            {
                int result = write(1, "The overload sensor has been tripped!\n", 38);
                if (result < 0)
                    return 1;
                safety.overload_msg_sent = 1;
            }
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
    return (state->open_button < 2 && state->close_button < 2 && state->emergency_mode < 2 &&
            state->emergency_stop < 2 && state->overload < 2 &&
            state->individual_service_mode < 2 && state->door_obstruction < 2);
}

bool is_shm_status_valid(const car_shared_mem *state)
{
    const char *statuses[] = {"Opening", "Open", "Closing", "Closed", "Between"};
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
    bool is_valid_obstruction_state =
        strcmp(state->status, "Closing") == 0 || strcmp(state->status, "Opening") == 0;
    return state->door_obstruction == 1 && is_valid_obstruction_state;
}

bool is_shm_data_valid(const car_shared_mem *state)
{
    return (is_shm_status_valid(state) && is_valid_floor(state->current_floor) &&
            is_valid_floor(state->destination_floor) && is_shm_int_fields_valid(state) &&
            is_shm_obstruction_valid(state)

    );
}
