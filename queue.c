#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "queue.h"

void queue_init(queue_t *queue)
{
	queue->head = NULL;
}

void queue_deinit(queue_t *queue)
{
    node_t *current = queue->head;
    node_t *next;
    
    while (current != NULL) {
        next = current->next;  // Save the next node
		node_deinit(&current);
        current = next;
    }
    
    queue->head = NULL;  // Optionally, reset the queue's head to NULL
}

void node_init(node_t **node, char *floor, floor_direction_t direction, node_t *next)
{
	*node = (node_t *) calloc(1, sizeof(node_t));
	(*node)->data.floor = strdup(floor);
	(*node)->data.direction = direction;
	(*node)->next = next;
}

void node_deinit(node_t **node)
{
	free((*node)->data.floor);
	free(*node);
	*node = NULL;
}

node_t *get_last(queue_t *queue)
{
	if (queue->head == NULL) return NULL;

	node_t *current_node = queue->head;
	while (current_node->next != NULL) {
		current_node = current_node->next;
	}
	return current_node;
}

void enqueue(queue_t *queue, char *floor, floor_direction_t direction)
{
	node_t *new_node;
	node_init(&new_node, floor, direction, NULL);

	if (queue->head == NULL) 
		queue->head = new_node;
	else
	{
		node_t *last_node = get_last(queue);
		last_node->next = new_node;
	}
}

char *dequeue(queue_t *queue)
{
	if (queue->head == NULL) return NULL;
	node_t *head = queue->head;
	char *result = strdup(head->data.floor);
	queue->head = head->next;
	node_deinit(&head);
	return result;
}
