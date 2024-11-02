#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "posix.h"

#define WITH_LOCK_AND_BROADCAST(state, code)                                   \
    do                                                                         \
    {                                                                          \
        pthread_mutex_lock(&(state)->mutex);                                   \
        code;                                                                  \
        pthread_cond_broadcast(&(state)->cond);                                \
        pthread_mutex_unlock(&(state)->mutex);                                 \
    } while (0)

void reset_shm(car_shared_mem *s)
{
    pthread_mutex_lock(&s->mutex);
    size_t offset = offsetof(car_shared_mem, current_floor);
    memset((char *)s + offset, 0, sizeof(*s) - offset);

    strcpy(s->status, "Closed");
    strcpy(s->current_floor, "1");
    strcpy(s->destination_floor, "1");
    pthread_mutex_unlock(&s->mutex);
}

bool connect_to_car(car_shared_mem **state, const char *shm_name, int *fd)
{
    *fd = shm_open(shm_name, O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (*fd < 0)
    {
        return false;
    }

    *state =
        mmap(0, sizeof(**state), PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*state == NULL)
    {
        return false;
    }

    return true;
}

void init_shm(car_shared_mem *s)
{
    pthread_mutexattr_t mutattr;
    pthread_mutexattr_init(&mutattr);
    pthread_mutexattr_setpshared(&mutattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&s->mutex, &mutattr);
    pthread_mutexattr_destroy(&mutattr);

    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&s->cond, &condattr);
    pthread_condattr_destroy(&condattr);

    reset_shm(s);
}

bool create_shared_mem(car_shared_mem **shm, int *fd, const char *name)
{
    // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(name);

    // Create the shared memory object, allowing read-write access by all users,
    // and saving the resulting file descriptor in fd
    *fd = shm_open(name, O_RDWR | O_CREAT,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (*fd == -1)
    {
        *shm = NULL;
        return false;
    }

    // Set the capacity of the shared memory object via ftruncate.
    if (ftruncate(*fd, sizeof(car_shared_mem)))
    {
        *shm = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the
    // address in *shm. If mapping fails, return false.
    *shm = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE,
                MAP_SHARED, *fd, 0);
    if (*shm == MAP_FAILED)
    {
        return false;
    }

    return true;
}

void set_status(car_shared_mem *state, const char *status)
{
    WITH_LOCK_AND_BROADCAST(state, strcpy(state->status, status));
}

void set_destination_floor(car_shared_mem *state, const char *floor)
{
    WITH_LOCK_AND_BROADCAST(state, strcpy(state->destination_floor, floor));
}

void set_open_button(car_shared_mem *state, uint8_t value)
{
    WITH_LOCK_AND_BROADCAST(state, state->open_button = value);
}

void set_close_button(car_shared_mem *state, uint8_t value)
{
    WITH_LOCK_AND_BROADCAST(state, state->close_button = value);
}

void set_emergency_stop(car_shared_mem *state, uint8_t value)
{
    WITH_LOCK_AND_BROADCAST(state, state->emergency_stop = value);
}

void set_service_mode(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    if (value == 1)
    {
        state->emergency_mode = 0;
    }
    state->individual_service_mode = value;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mutex);
}

bool open_button_is(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    bool result = state->open_button == value;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

bool close_button_is(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    bool result = state->close_button == value;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

bool status_is(car_shared_mem *state, const char *status)
{
    pthread_mutex_lock(&state->mutex);
    bool result = strcmp(state->status, status) == 0;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

bool service_mode_is(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    bool result = state->individual_service_mode == value;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

char *get_shm_name(const char *car_name)
{
    char *shm_name = malloc(sizeof(car_name) + 5);
    sprintf(shm_name, "/car%s", car_name);
    return shm_name;
}
