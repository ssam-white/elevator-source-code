#pragma once

#include <arpa/inet.h>

#include "queue.h"
#include "tcpip.h"

/*
 * Structure representing a connection to a car, including the socket
 * descriptor, car name, floor information, and a priority-based queue for
 * scheduling car requests.
 */
typedef struct car_connection
{
    int sd;              // Socket descriptor for the car connection
    char *name;          // Name of the car
    char *lowest_floor;  // Lowest floor the car can access
    char *highest_floor; // Highest floor the car can access
    queue_t queue;       // Queue for messages related to the car
} car_connection_t;

/*
 * Structure representing the elevator controller, including the server socket
 * descriptor, socket information, file descriptor set for monitoring, and
 * an array of car connections.
 */
typedef struct controller
{
    int server_sd;              // Socket descriptor for the server socket
    struct sockaddr_in sock;    // Socket address structure
    fd_set readfds;             // Set of file descriptors for reading
    int max_sd;                 // Maximum socket descriptor
    size_t num_car_connections; // Number of active car connections
    car_connection_t car_connections[BACKLOG]; // Array of car connections
} controller_t;

/* Function prototypes for managing the controller and car connections */
void controller_init(controller_t *);           // Initialize the controller
void controller_deinit(controller_t *);         // Deinitialize the controller
void car_connection_init(car_connection_t *);   // Initialize a car connection
void car_connection_deinit(car_connection_t *); // Deinitialize a car connection

// Add a car connection
void add_car_connection(controller_t *, int, const char *, const char *,
                        const char *);
// Handle a call to the controller
void handle_call(controller_t *, int, const char *, const char *);
// Handle messages from the server
void handle_server_message(controller_t *, char *, int);
// Handle messages from a car connection
void handle_car_connection_message(controller_t *, car_connection_t *, char *);
// Process incoming messages
void handle_incoming_messages(controller_t *);
// Schedule a car for a specific floor
void schedule_car(car_connection_t *, const char *, const char *);
