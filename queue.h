#pragma once

#include <stdbool.h>

/*
 * This header file defines the data structures and function prototypes
 * for a queue implementation used in an elevator system. More details
 * about the implementation can be found in queue.c.
 */

/*
 * Enumeration to represent the direction of a floor request
 */
typedef enum
{
    UP_FLOOR = 1,   // Indicates a request to go up to a higher floor
    DOWN_FLOOR = 0, // Indicates a request to go down to a lower floor
} floor_direction_t;

/*
 * Structure to hold the data for each node in the queue
 */
typedef struct
{
    floor_direction_t direction; // Direction of the floor request (up or down)
    bool been_displayed;         // Flag indicating whether the request has been
                                 // displayed
    char *floor;                 // The floor number represented as a string
} node_data_t;

/*
 * Structure representing a node in the singly linked list
 */
typedef struct node
{
    node_data_t data;  // Data contained in the node
    struct node *next; // Pointer to the next node in the queue
} node_t;

/*
 * Structure for the queue itself, holding a pointer to the head node
 */
typedef struct
{
    node_t *head; // Pointer to the head node of the queue
} queue_t;

// Function prototypes for queue operations
void queue_init(queue_t *);
void queue_deinit(queue_t *);
void node_init(node_t **, const char *, floor_direction_t, node_t *);
void node_deinit(node_t **);
void enqueue(queue_t *, const char *, floor_direction_t);
void dequeue(queue_t *);
void print_queue(queue_t *);
void enqueue_pair(queue_t *, const char *, const char *);
char *queue_peek(queue_t *);
node_t *queue_peek_undisplayed(queue_t *);
char *queue_prev_floor(queue_t *);
char *queue_get_undisplayed(queue_t *);
bool queue_empty(const queue_t *);
