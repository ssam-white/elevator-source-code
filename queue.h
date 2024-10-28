#pragma once

/*
 * Queue Header File
 *
 * This header file defines the data structures and function prototypes
 * for a simple queue implementation using linked nodes. The queue supports
 * a first-in, first-out (FIFO) mechanism for managing floor requests.
 *
 * Enumerations:
 * - floor_direction_t: Represents the direction of movement (UP or DOWN).
 * 
 * Data Structures:
 * - node_data_t: Contains information about a floor request, including
 *   the direction of movement and the floor number as a string.
 * 
 * - node_t: Represents a single node in the queue. Each node holds
 *   data of type `node_data_t` and a pointer to the next node in the queue.
 * 
 * - queue_t: Represents the queue itself, containing a pointer to
 *   the head node of the queue.
 *
 * Function Prototypes:
 * - queue_init: Initializes a queue, setting its head to NULL.
 * - queue_deinit: Frees all nodes and their associated data in the queue.
 * - node_init: Allocates memory for a new node and initializes it with
 *   provided floor and direction.
 * - node_deinit: Frees the memory allocated for a node and its data.
 * - get_last: Returns a pointer to the last node in the queue.
 * - enqueue: Adds a new node to the end of the queue with the specified
 *   floor and direction.
 * - dequeue: Removes and returns the floor string from the front of the queue.
 *   The caller is responsible for freeing the returned string after use.
 */

typedef enum {
    UP_FLOOR = 1,
    DOWN_FLOOR = 0,
} floor_direction_t;

typedef struct {
    floor_direction_t direction; // Direction of the floor request
    char *floor;                 // The floor number as a string
} node_data_t;

typedef struct node {
    node_data_t data;           // Data contained in the node
    struct node *next;          // Pointer to the next node in the queue
} node_t;

typedef struct {
    node_t *head;               // Pointer to the head node of the queue
} queue_t;

// Function prototypes
void queue_init(queue_t *);
void queue_deinit(queue_t *);
void node_init(node_t **, char *, floor_direction_t, node_t *);
void node_deinit(node_t **);
node_t *get_last(queue_t *);
void enqueue(queue_t *, char *, floor_direction_t);
void dequeue(queue_t *);
void print_queue(queue_t *);
void enqueue_pair(queue_t *, char *, char *);
char *peek(queue_t *);
