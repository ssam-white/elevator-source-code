#pragma once

#include <stdbool.h>

typedef enum
{
    UP_FLOOR = 1,
    DOWN_FLOOR = 0,
} floor_direction_t;

typedef struct
{
    floor_direction_t direction; // Direction of the floor request
    char *floor;                 // The floor number as a string
} node_data_t;

typedef struct node
{
    node_data_t data;  // Data contained in the node
    struct node *next; // Pointer to the next node in the queue
} node_t;

typedef struct
{
    node_t *head; // Pointer to the head node of the queue
    bool between;
} queue_t;

// Function prototypes
void queue_init(queue_t *);
void queue_deinit(queue_t *);
void node_init(node_t **, const char *, floor_direction_t, node_t *);
void node_deinit(node_t **);
node_t *get_last(queue_t *);
void enqueue(queue_t *, const char *, floor_direction_t);
void dequeue(queue_t *);
void print_queue(queue_t *);
void enqueue_pair(queue_t *, const char *, const char *);
char *queue_peek_current(queue_t *);
void queue_set_between(queue_t *, bool);
node_t *queue_get_current(queue_t *);
