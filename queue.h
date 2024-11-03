#pragma once

#include <stdbool.h>

/*
 * This header file defines the data structure and function prototypes
 * for the car module. The car module handles individual car (elevator) behavior
 * including communication with the controller, managing door operations,
 * updating current floor level, and tracking state through shared memory.
 * Details about the specific implementation can be found in `car.c`.
 */

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
    UP_FLOOR = 1,   /* Indicates a request to go up to a higher floor */
    DOWN_FLOOR = 0, /* Indicates a request to go down to a lower floor */
} floor_direction_t;

/*
 * Structure to hold the data for each node in the queue
 */
typedef struct node_data
{
    /* For determining if the node is an up floor or a down floor. */
    floor_direction_t direction;
    /* Flag for determining if this floor has been sent in a message to the
     * caller. */
    bool been_displayed;
    /* Floor represented by a string. */
    char *floor;
} node_data_t;

/*
 * Structure representing a node containing some data and a link to the next
 * node  in the singly linked list.
 */
typedef struct node
{
    node_data_t data;
    struct node *next;
} node_t;

/*
 * Structure for the queue itself, holding a pointer to the head node
 */
typedef struct queue
{
    /* Reference to the first node in the queue */
    node_t *head;
} queue_t;

/* Function prototypes for queue operations */
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
