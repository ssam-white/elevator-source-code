#include <errno.h> /* Error definitions, e.g., ETIMEDOUT */
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "car.h"
#include "global.h"
#include "posix.h"
#include "tcpip.h"

/*
 * This file contains the core functionality for the car component, which uses a
 * multi-threaded approach to manage various responsibilities. Each key task is
 * handled by a dedicated thread:
 *   - A thread for managing door operations.
 *   - A thread for updating the car's level.
 *   - A thread for handling controller updates.
 *   - A thread for receiving messages from the controller.
 *
 * The main thread is responsible for establishing and maintaining the server
 * connection, simplifying concurrency management by centralizing the connection
 * handling. This allows each worker thread to focus on specific tasks while the
 * main thread oversees the connection lifecycle and ensures coordination
 * between the car and the controller.
 *
 * Signal handling is also implemented here to support clean shutdowns and
 * resource cleanup.
 */

/*
 * Thread cleanup function for unlocking a mutex
 */
static void cleanup_mutex_thread(void *arg)
{
    pthread_mutex_unlock((pthread_mutex_t *)arg);
}

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
    /* Check if the correct number of command line arguments was given */
    if (!is_args_valid(argc))
    {
        return 1;
    }

    /* Setup the signal handler. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    /* Register signal handler for SIGINT */
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    /* Ignore SIGPIPE so that the program doesn't terminate if messages fail to
     * send */
    signal(SIGPIPE, SIG_IGN);

    /* Initialize car and threads */
    car_t car;
    car_init(&car, argv[1], argv[2], argv[3], argv[4]);

    pthread_create(&car.level_thread, NULL, handle_level, &car);
    pthread_create(&car.door_thread, NULL, handle_doors, &car);

    /* Main loop to maintain connection and handle service/emergency modes */
    while (keep_running)
    {
        /*
         * If the car hasn't been put into emergency or individual service mode
         * and isn't connected to the controller, then connect the car
         */
        if (should_maintain_connection(&car) && !car.connected_to_controller &&
            connect_to_controller(&car.server_sd, &car.server_addr))
        {
            handle_initial_connection(&car);

            pthread_create(&car.updater_thread, NULL, handle_updater, &car);
            pthread_create(&car.receiver_thread, NULL, handle_receiver, &car);
        }
        sleep_delay(&car);
    }

    /* Cleanup threads and car resources */
    pthread_cancel(car.level_thread);
    pthread_cancel(car.door_thread);
    if (car.connected_to_controller)
    {
        pthread_cancel(car.receiver_thread);
        pthread_cancel(car.updater_thread);
    }

    pthread_join(car.level_thread, NULL);
    pthread_join(car.door_thread, NULL);
    if (car.connected_to_controller)
    {
        pthread_join(car.receiver_thread, NULL);
        pthread_join(car.updater_thread, NULL);
    }

    car_deinit(&car);

    return 0;
}

/*
 * Thread function to handle door operations
 */
void *handle_doors(void *arg)
{
    car_t *car = (car_t *)arg;

    while (1)
    {
        pthread_mutex_lock(&car->state->mutex);
        pthread_cleanup_push(cleanup_mutex_thread, &car->state->mutex);
        pthread_cond_wait(&car->state->cond, &car->state->mutex);
        pthread_cleanup_pop(1);

        /* Dont allow door buttons to do anything if the car is between floors.
         */
        if (status_is(car->state, "Between"))
            continue;

        /* If the open button was pressed, set open button to 0 and cycle doors
         * making sure to check if the cycling was interupted at any point. */
        if (open_button_is(car->state, 1))
        {
            set_open_button(car->state, 0);
            open_doors(car);
            if (sleep_delay_cond(car) == 0 && service_mode_is(car->state, 0))
            {
                close_doors(car);
            }
        }

        /* Do the same for the close button */
        if (close_button_is(car->state, 1))
        {
            set_close_button(car->state, 0);
            close_doors(car);
        }

        /* Check if a thread cancelation was requested */
        pthread_testcancel();
    }
}

/*
 * Thread function to handle car level changes
 */
void *handle_level(void *arg)
{
    car_t *car = (car_t *)arg;

    while (1)
    {
        /* Wait on the condition variable and make sure to unlock the mutex if
         * the thread was canceled while waiting */
        pthread_mutex_lock(&car->state->mutex);
        pthread_cleanup_push(cleanup_mutex_thread, &car->state->mutex);
        pthread_cond_wait(&car->state->cond, &car->state->mutex);
        pthread_cleanup_pop(1);

        /* If the current and destination floors are different then move the car
         * towards the destination floor. */
        if (cdcmp_floors(car->state) != 0)
        {
            /* Acquire the mutex to check if the destination floor is in range
             * of the car. */
            pthread_mutex_lock(&car->state->mutex);
            int bounds_check =
                bounds_check_floor(car, car->state->destination_floor);
            pthread_mutex_unlock(&car->state->mutex);

            /* If the destination floor is out of range, put it back in range by
             * setting it to the lowest or highest floor and set the status back
             * to Closed. */
            if (bounds_check == -1)
            {
                set_destination_floor(car->state, car->lowest_floor);
                set_status(car->state, "Closed");
                continue;
            }
            else if (bounds_check == 1)
            {
                set_destination_floor(car->state, car->highest_floor);
                set_status(car->state, "Closed");
                continue;
            }

            /* While the current and destination floors are different, move the
             * current floor towards the destination floor. */
            set_status(car->state, "Between");
            while (cdcmp_floors(car->state) != 0)
            {
                sleep_delay(car);
                /* Find out witch direction the destination floor is in by
                 * comparing it to the current floor to determina whether to
                 * increment the current floor or decrement the current floor.
                 */
                int compare_floors = cdcmp_floors(car->state);
                if (compare_floors == 1)
                {
                    pthread_mutex_lock(&car->state->mutex);
                    /* If the current floor was successfully incremented them
                     * broadcast on the condition variable. */
                    if (increment_floor(car->state->current_floor) == 0)
                    {
                        pthread_cond_broadcast(&car->state->cond);
                    }
                    pthread_mutex_unlock(&car->state->mutex);
                }
                else if (compare_floors == -1)
                {
                    pthread_mutex_lock(&car->state->mutex);
                    /* Broadcast on the condition variable if the floor was
                     * successfully decremented. */
                    if (decrement_floor(car->state->current_floor) == 0)
                    {
                        pthread_cond_broadcast(&car->state->cond);
                    }
                    pthread_mutex_unlock(&car->state->mutex);
                }

                /* If the current and destination floors are now the same, set
                 * the status to closed before exiting the loop. */
                if (cdcmp_floors(car->state) == 0)
                {
                    set_status(car->state, "Closed");
                    break;
                }
            }

            /* Cycle the doors if the car isn't in individual service mode. We
             * dont want to do this inside the above loop because there a are
             * times when the caller is already on the destination floor and the
             * car doesn't have to move. */
            if (service_mode_is(car->state, 0))
            {
                open_doors(car);
                sleep_delay(car);
                close_doors(car);
            }
        }

        /* set a safe cancelation point for the thread to prevent deadlocks */
        pthread_testcancel();
    }
}

/*
 * Initialize car structure and create shared memory
 */
void car_init(car_t *car, const char *name, const char *lowest_floor,
              const char *highest_floor, const char *delay)
{
    car->name = name;
    car->shm_name = get_shm_name(car->name);
    car->lowest_floor = lowest_floor;
    car->highest_floor = highest_floor;
    car->delay = (uint8_t)atoi(delay);
    car->connected_to_controller = false;
    car->server_sd = -1;

    /* Create shared memory for car state */
    if (!create_shared_mem(&car->state, &car->fd, car->shm_name))
    {
        perror("Failed to create shared object");
        exit(1);
    }

    /* Set the current and destination floors to the cars lowest floor */
    init_shm(car->state);
    pthread_mutex_lock(&car->state->mutex);
    strcpy(car->state->current_floor, car->lowest_floor);
    strcpy(car->state->destination_floor, car->lowest_floor);
    pthread_cond_broadcast(&car->state->cond);
    pthread_mutex_unlock(&car->state->mutex);
}

/*
 * Deinitialize car and clean up resources
 */
void car_deinit(car_t *car)
{
    /* Close the shared memory object */
    munmap(car->state, sizeof(car_shared_mem));
    shm_unlink(car->shm_name);
    car->state = NULL;

    /* Deinitialize other fields. */
    car->name = NULL;
    free(car->shm_name);
    car->highest_floor = NULL;
    car->lowest_floor = NULL;
    car->delay = 0;
}

/*
 * Open car doors and update state.
 */
void open_doors(car_t *car)
{
    set_status(car->state, "Opening");
    sleep_delay(car);
    set_status(car->state, "Open");
}

/*
 * close car doors and update state
 */
void close_doors(car_t *car)
{
    if (status_is(car->state, "Closed"))
        return;
    set_status(car->state, "Closing");
    sleep_delay(car);
    set_status(car->state, "Closed");
}

/*
 * Sleeps s untill car.delay nannoseconds has passes or one of the dorr buttons
 * was pressed.
 */
int sleep_delay_cond(car_t *car)
{
    /* Create a timestamp with car->delay */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += car->delay * 1000000;

    while (true)
    {
        pthread_mutex_lock(&car->state->mutex);
        int result =
            pthread_cond_timedwait(&car->state->cond, &car->state->mutex, &ts);
        uint8_t open_button = car->state->open_button;
        uint8_t close_button = car->state->close_button;
        pthread_mutex_unlock(&car->state->mutex);

        /* Check if a door button was pressed */
        if (open_button == 1 || close_button == 1)
        {
            return -1;
        }

        /* If the wait timed out, just return */
        if (result == ETIMEDOUT)
        {
            return 0;
        }
    }
}

/*
 * Checks to see if a given floor is between the lowest and highest floors of
 * the given car.
 */
int bounds_check_floor(const car_t *car, const char *floor)
{
    int floor_number = floor_to_int(floor);
    int lowest_floor_number = floor_to_int(car->lowest_floor);
    int highest_floor_number = floor_to_int(car->highest_floor);

    if (floor_number < lowest_floor_number)
        return -1;
    else if (floor_number > highest_floor_number)
        return 1;
    else
        return 0;
}

/*
 * Compares the cars current and destination floors. Returns -1 if the current
 * floor is below the destination floor and returns 1 if the current floor is
 * above the destination floor. Returns 0 if the current floor is the
 * destination floor
 */
int cdcmp_floors(car_shared_mem *state)
{
    /* Acquire the mutex and convert the floors to integers */
    pthread_mutex_lock(&state->mutex);
    int cf_number = floor_to_int(state->current_floor);
    int df_number = floor_to_int(state->destination_floor);
    pthread_mutex_unlock(&state->mutex);

    /* Compare the floors and return the corrisponding result. */
    if (cf_number > df_number)
        return -1;
    else if (cf_number < df_number)
        return 1;
    else
        return 0;
}

/*
 * Thread function for handling incoming messages from the controller.
 */
void *handle_receiver(void *arg)
{
    car_t *car = (car_t *)arg;

    while (1)
    {
        /* Wait for a message from the controller */
        char *message = receive_msg(car->server_sd);

        /* Begin tokenizing the string to confirm that its form is valid */
        char *saveptr;
        const char *message_type = strtok_r(message, " ", &saveptr);

        /* Confirm the message is a floor request. */
        if (strcmp(message_type, "FLOOR") == 0)
        {
            /* continue tokenizing the message to extract the floor. */
            const char *floor = strtok_r(NULL, " ", &saveptr);

            /* Compare the requested floor with the current destination floor.
             */
            pthread_mutex_lock(&car->state->mutex);
            int result = strcmp(car->state->destination_floor, floor);
            pthread_mutex_unlock(&car->state->mutex);

            /* If the car is already on the requested floor then cycle the
             * doors. */
            if (result == 0)
            {
                open_doors(car);
                sleep_delay(car);
                close_doors(car);
            }
            else
            {
                /* Otherwise send the car to its destination. The level thread
                 * will take over from here. */
                set_status(car->state, "Beterrn");
                set_destination_floor(car->state, floor);
            }
        }
        free(message);
    }
}

/*
 * Thread function that keeps the controller updated about the internal state of
 * the car.
 */
void *handle_updater(void *arg)
{
    car_t *car = (car_t *)arg;

    while (1)
    {
        /* Acquire the mutex and wait on the condition variable ensuring to
         * release the mutex if the thread is canceled while waiting. */
        pthread_mutex_lock(&car->state->mutex);
        pthread_cleanup_push(cleanup_mutex_thread, &car->state->mutex);
        pthread_cond_wait(&car->state->cond, &car->state->mutex);
        pthread_cleanup_pop(1);

        /* Acquire service mode and emergency mode from the car state. */
        pthread_mutex_lock(&car->state->mutex);
        uint8_t service_mode = car->state->individual_service_mode;
        uint8_t emergency_mode = car->state->emergency_mode;
        pthread_mutex_unlock(&car->state->mutex);

        /* If emergency mode is on, alert the controller and close the
         * connection. */
        if (emergency_mode == 1)
        {
            send_message(car->server_sd, "EMERGENCY");
            // close(car->server_sd);
            break;
        }
        /* If individual service mode is on, alert the controller and set
         * car->connected_to_controller to false. */
        else if (service_mode == 1)
        {
            send_message(car->server_sd, "INDIVIDUAL SERVICE");
            car->connected_to_controller = false;
            break;
        }
        else /* Otherwise signal the controller with the cars new internal
                state. */
        {
            signal_controller(car);
        }

        /* Test to se if the thread has been requested to cancel. */
        pthread_testcancel();
    }

    return NULL;
}

/*
 * Signals the controller with the cars new internal state.
 */
void signal_controller(car_t *car)
{
    send_message(car->server_sd, "STATUS %s %s %s", car->state->status,
                 car->state->current_floor, car->state->destination_floor);
}

/*
 * Sleep for car->delay.
 */
void sleep_delay(const car_t *car)
{
    /* create a time stamp with the cars delay in it */
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = car->delay * 1000000;
    nanosleep(&req, NULL);
}

/*
 * Checks if the car should connect to the controller based on what mode the car
 * is in.
 */
bool should_maintain_connection(car_t *car)
{
    /* Return false if the car is in emergency mode or individual service mode
     * otherwise return true. */
    pthread_mutex_lock(&car->state->mutex);
    bool service_on = car->state->individual_service_mode == 1;
    bool emergency_on = car->state->emergency_mode == 1;
    pthread_mutex_unlock(&car->state->mutex);

    if (service_on || emergency_on)
        return false;
    else
        return true;
}

/*
 * Used when the car initially connects to the controller to signal with the
 * name of the car and its initial state. Also sets the connected_to_controller
 * flag to true.
 */
void handle_initial_connection(car_t *car)
{
    car->connected_to_controller = true;
    send_message(car->server_sd, "CAR %s %s %s", car->name, car->lowest_floor,
                 car->highest_floor);
    signal_controller(car);
}

/*
 * Validates command line arguments.
 */
bool is_args_valid(int argc)
{
    if (argc != 5)
    {
        fprintf(
            stderr,
            "Error: Incorrect number of command line arguments. Expected 4, "
            "got %d.\n",
            argc - 1);
        return false;
    }
    return true;
}
