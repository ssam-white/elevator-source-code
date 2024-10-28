#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "global.h"
#include "queue.h"

// Initializes the queue by setting the head to NULL.
void queue_init(queue_t *queue)
{
    queue->head = NULL;
}

// Deinitializes the queue by freeing all allocated nodes and their data.
void queue_deinit(queue_t *queue)
{
    node_t *current = queue->head; // Start with the head of the queue.
    node_t *next;                   // Pointer to hold the next node.

    // Iterate through the list and free each node.
    while (current != NULL) {
        next = current->next;       // Save the next node for iteration.
        node_deinit(&current);      // Free the current node and its data.
        current = next;             // Move to the next node.
    }

    queue->head = NULL;             // Optionally reset the queue's head to NULL.
}

// Initializes a new node with the specified floor and direction.
// Allocates memory for the node and duplicates the floor string.
void node_init(node_t **node, char *floor, floor_direction_t direction, node_t *next)
{
    // Allocate memory for the new node.
    *node = (node_t *) calloc(1, sizeof(node_t));
    
    // Duplicate the floor string to ensure the node owns its data.
    (*node)->data.floor = strdup(floor);
    
    // Set the direction and next pointer for the new node.
    (*node)->data.direction = direction;
    (*node)->next = next;
}

// Deinitializes a node by freeing its allocated memory.
void node_deinit(node_t **node)
{
    // Check if the node pointer and the node itself are valid.
    if (node && *node) {
        free((*node)->data.floor); // Free the duplicated floor string.
        free(*node);               // Free the node structure itself.
        *node = NULL;              // Set the pointer to NULL after freeing.
    }
}

// Retrieves the last node in the queue.
node_t *get_last(queue_t *queue)
{
    // Return NULL if the queue is empty.
    if (queue->head == NULL) return NULL;

    node_t *current_node = queue->head; // Start from the head of the queue.

    // Traverse to the last node.
    while (current_node->next != NULL) {
        current_node = current_node->next;
    }
    
    return current_node; // Return the last node found.
}

// Adds a new node to the end of the queue.
void enqueue(queue_t *queue, char *floor, floor_direction_t direction)
{
    node_t *new_node; // Pointer to hold the new node.

    node_init(&new_node, floor, direction, NULL);
    if (queue->head == NULL) 
        queue->head = new_node;
    else {
		node_t *current_node = queue->head; // Start from the head of the queue.
		while (current_node->next != NULL) {
			if (strcmp(current_node->data.floor, new_node->data.floor) == 0) return;
			current_node = current_node->next;
		}
		current_node->next = new_node; // Append the new node to the end.
    }
}

void dequeue(queue_t *queue)
{
    if (queue->head == NULL) return;
    node_t *head = queue->head;
    queue->head = head->next;
	node_deinit(&head);
}

void print_queue(queue_t *queue)
{
	node_t *current = queue->head;
	while (current != NULL) {
		char *direction = current->data.direction == UP_FLOOR ? "U" : "D";
		printf("%s%s ", direction, current->data.floor);
		current = current->next;
	}
	printf("\n");
}

void enqueue_pair(queue_t *queue, char *source_floor, char *destination_floor)
{
	int source_number = floor_to_int(source_floor);
	int destination_number = floor_to_int(destination_floor);
	floor_direction_t direction = source_number > destination_number ? DOWN_FLOOR : UP_FLOOR;

	enqueue(queue, source_floor, direction);
	enqueue(queue, destination_floor, direction);
}

char *peek(queue_t *queue)
{
	return queue->head->data.floor;
}
