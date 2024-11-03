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

/*
 * This is a set of function implementations made to aid in working with the
 * car_shared_mem data structure. It should reduce a portion of the repatition
 * that is acquiring mutexes, connecting to and creating shared memory objects.
 */

#include "posix.h"

/*
 * Pre processor macro to reduce repatition when acquiring the mutex and
 * broadcasting on the condition variable
 */
#define WITH_LOCK_AND_BROADCAST(state, code)                                   \
    do                                                                         \
    {                                                                          \
        pthread_mutex_lock(&(state)->mutex);                                   \
        code;                                                                  \
        pthread_cond_broadcast(&(state)->cond);                                \
        pthread_mutex_unlock(&(state)->mutex);                                 \
    } while (0)

/*
 * Sets the shared memory objects fields to default values.
 */
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

/*
 * Opens the shared memory object and maps its contents to the state pointer
 */
bool connect_to_car(car_shared_mem **state, const char *shm_name, int *fd)
{
    /* Open the shared memory object */
    *fd = shm_open(shm_name, O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (*fd < 0)
    {
        return false;
    }

    /* Map the shared memory object to `state` */
    *state =
        mmap(0, sizeof(**state), PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*state == NULL)
    {
        return false;
    }

    return true;
}

/*
 * Initialize the mutex and condition variables and sets the remaining fields to
 * defult values.
 */
void init_shm(car_shared_mem *s)
{
    /* Initialise mutex */
    pthread_mutexattr_t mutattr;
    pthread_mutexattr_init(&mutattr);
    pthread_mutexattr_setpshared(&mutattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&s->mutex, &mutattr);
    pthread_mutexattr_destroy(&mutattr);

    /* Initialise condition variable */
    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&s->cond, &condattr);
    pthread_condattr_destroy(&condattr);

    /* Set the rest of the fields to defult values */
    reset_shm(s);
}

/*
 * Creates the shared memory object
 */
bool create_shared_mem(car_shared_mem **shm, int *fd, const char *name)
{
    /* Remove any previous instance of the shared memory object, if it exists.
     */
    shm_unlink(name);

    /* Create the shared memory object, allowing read-write access by all users,
     * and saving the resulting file descriptor in fd */
    *fd = shm_open(name, O_RDWR | O_CREAT,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (*fd == -1)
    {
        *shm = NULL;
        return false;
    }

    /* Set the capacity of the shared memory object via ftruncate. */
    if (ftruncate(*fd, sizeof(car_shared_mem)))
    {
        *shm = NULL;
        return false;
    }

    /* Otherwise, attempt to map the shared memory via mmap, and save the
     * adress in *shm. If mapping fails, return false. */
    *shm = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE,
                MAP_SHARED, *fd, 0);
    if (*shm == MAP_FAILED)
    {
        return false;
    }

    return true;
}

/*
 * Acquires the mutex and sets the string in state->status to status.
 */
void set_status(car_shared_mem *state, const char *status)
{
    WITH_LOCK_AND_BROADCAST(state, strcpy(state->status, status));
}

/*
 * Acquires the mutex and sets the destination floor in the shared memory object
 * to floor
 */
void set_destination_floor(car_shared_mem *state, const char *floor)
{
    WITH_LOCK_AND_BROADCAST(state, strcpy(state->destination_floor, floor));
}

/*
 * Acquires the mutex and sets the open button flag.
 */
void set_open_button(car_shared_mem *state, uint8_t value)
{
    WITH_LOCK_AND_BROADCAST(state, state->open_button = value);
}

/*
 * Acquires the mutex and sets the close button flag in the shared memory
 * object.
 */
void set_close_button(car_shared_mem *state, uint8_t value)
{
    WITH_LOCK_AND_BROADCAST(state, state->close_button = value);
}

/*
 * Acquires the mutex and set the emergency stop flag.
 */
void set_emergency_stop(car_shared_mem *state, uint8_t value)
{
    WITH_LOCK_AND_BROADCAST(state, state->emergency_stop = value);
}

/*
 * Acquires the mutex and sets the individual service mode flag in the shared
 * memory object. It also ensures the emergency mode flag isn't set if the
 * individual service mode flag is set.
 */
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

/*
 * Acquires the mutex and checks if the open button in shared memory is equal to
 * a value
 */
bool open_button_is(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    bool result = state->open_button == value;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

/*
 * Acquires the mutex and compares the close button flag in shared memory to a
 * value returning the result.
 */
bool close_button_is(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    bool result = state->close_button == value;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

/*
 * Acquires the mutex and compares the status string in shared memory to another
 * string returning whether the two are equal or not.
 */
bool status_is(car_shared_mem *state, const char *status)
{
    pthread_mutex_lock(&state->mutex);
    bool result = strcmp(state->status, status) == 0;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

/*
 * Acquires the mutex and compares the service mode flag in shared memory to a
 * given value and returns whether the two are equal or not.
 */
bool service_mode_is(car_shared_mem *state, uint8_t value)
{
    pthread_mutex_lock(&state->mutex);
    bool result = state->individual_service_mode == value;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

/*
 * Takes a car name string and coppies it onto the heap, prepending it with the
 * characters '/car' and returns a pointer to the memory. The pointer returned
 * by this function must be freed lter on.
 */
char *get_shm_name(const char *car_name)
{
    char *shm_name = malloc(sizeof(car_name) + 5);
    sprintf(shm_name, "/car%s", car_name);
    return shm_name;
}
