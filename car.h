#pragma once

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>

#include "posix.h"

/*
 * Structure representing a car in the elevator system. Contains information
 * on server connection, mutex and condition variables for syncronisation, car
 * details, thread handlers, and shared memory for tracking car state.
 */

typedef struct car
{
    int server_sd;                  // Socket descriptor for server connection
    struct sockaddr_in server_addr; // Server address information
    const char *name;               // Name identifier for the car
    char *shm_name;                 // Shared memory name
    const char *lowest_floor;       // The lowest accessible floor for this car
    const char *highest_floor;      // The highest accessible floor for this car
    uint32_t delay;                 // Delay duration for certain operations
    pthread_t door_thread;          // Thread for handling door operations
    pthread_t level_thread;    // Thread for handling level (floor) operations
    pthread_t receiver_thread; // Thread for receiving messages from controller
    pthread_t updater_thread;  // Thread for updating the controller
    bool connected_to_controller; // Flag to indicate connection status
    int fd;                       // File descriptor for shared memory
    car_shared_mem *state; // Pointer to shared memory containing car state
} car_t;

/*
 * Function prototypes for initializing and deinitializing a car structure
 */

// Initialize a car with specified values.
void car_init(car_t *, const char *, const char *, const char *, const char *);
// Deinitializes car and cleans up resources
void car_deinit(car_t *);

/*
 * Thread handling functions for car operations
 */

// Function for managing door operations
void *handle_doors(void *);
// Function for managing current level updates
void *handle_level(void *);
// Function for receiving messages from the controller
void *handle_receiver(void *);
// Function for updating the controller
void *handle_updater(void *);

/*
 * Functions to manage door operations
 */

// Opens the car doors
void open_doors(car_t *);
// Closes the car doors
void close_doors(car_t *);

/*
 * Functions for managing delay
 */

// Sleeps for a delay unless the condition variable is signaled
int sleep_delay_cond(car_t *);
// Sleeps for the configured delay
void sleep_delay(const car_t *);

/*
 * Utility functions for floor bounds checking and comparison
 */

// Checks if a floor is within bounds of the cars lowest and highest floors.
int bounds_check_floor(const car_t *, const char *);
// Compares current and destination floors in shared memory.
int cdcmp_floors(car_shared_mem *);

/*
 * Controller communication functions
 */

// Updates the condroller with the cares current state.
void signal_controller(car_t *);
// Checks if car should be connected to the controller.
bool should_maintain_connection(car_t *);
// Handles steps needed after initial server connection.
void handle_initial_connection(car_t *);
// Validates command-line arguments
bool is_args_valid(int);
