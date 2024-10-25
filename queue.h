#pragma once

typedef enum {
	UP_FLOOR = 1,
	DOWN_FLOOR = 0,
} floor_direction_t;

typedef struct {
	floor_direction_t direction;
	char *floor;
} node_data_t;

typedef struct node {
	node_data_t data;
	struct node *next;
} node_t;

typedef struct {
	node_t *head;
} queue_t;

void queue_init(queue_t *);
void queue_deinit(queue_t *);
void node_init(node_t **, char *, floor_direction_t, node_t *);
void node_deinit(node_t **);

node_t *get_last(queue_t *);
void enqueue(queue_t *, char *, floor_direction_t);
char *dequeue(queue_t *);

