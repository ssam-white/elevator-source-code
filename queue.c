#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Single-Linked List Implementation for Elevator Queue
 *
 * This file implements a queue for an elevator system using a singly linked
 * list. Each node in the list represents a floor request and includes data for
 * the floor number, direction (up or down), and whether it has been displayed.
 * The list is designed for one-way traversal, meaning nodes only contain
 * references to the next node in line without backward links. This keeps each
 * node's memory footprint low and is well-suited to the problem since the list
 * is only traversed in a single direction.
 *
 * A linked list is a fine choice here—it gets the job done without
 * over-complicating things. It’s a simple approach that effectively handles
 * floor requests in the order they need to be processed, so we don't need
 * anything more complex.
 */

#include "global.h"
#include "queue.h"

/*
 * Initializes the queue by setting the head to NULL.
 */
void queue_init(queue_t *queue) { queue->head = NULL; }

/*
 * Deinitializes the queue by freeing all nodes, then setting head to NULL.
 */
void queue_deinit(queue_t *queue)
{
    node_t *current = queue->head;
    node_t *next;
    while (current != NULL)
    {
        next = current->next;
        node_deinit(&current); // Free each node in the queue
        current = next;
    }
    queue->head = NULL;
}

/*
 * Initializes a new node with the specified floor, direction, and next pointer.
 */
void node_init(node_t **node, const char *floor, floor_direction_t direction,
               node_t *next)
{
    *node =
        (node_t *)calloc(1, sizeof(node_t)); // Allocate memory for the new node

    (*node)->data.floor =
        strdup(floor); // Duplicate the floor string for the node
    (*node)->data.direction = direction; // Set the direction
    (*node)->data.been_displayed =
        false;            // Initially, the node has not been displayed
    (*node)->next = next; // Set the next pointer
}

/*
 * Deinitializes a node by freeing its memory and setting the node pointer to
 * NULL.
 */
void node_deinit(node_t **node)
{
    if (node && *node)
    {
        free((*node)->data.floor); // Free the allocated floor string
        free(*node);               // Free the node memory
        *node = NULL;              // Set the pointer to NULL
    }
}

/*
 * Enqueues a new node with the given floor and direction at the correct
 * position. Inserts the node while keeping the queue sorted based on floor
 * order within 'up' and 'down' direction blocks and ensures no duplicate
 * entries in the same direction if undisplayed.
 */
void enqueue(queue_t *queue, const char *floor, floor_direction_t direction)
{
    node_t *current = queue->head;
    node_t *prev = NULL;
    node_t *new_node = NULL;
    node_init(&new_node, floor, direction, NULL); // Initialize the new node
    int floor_number =
        floor_to_int(floor); // Convert floor to an integer for comparison

    /* Edge case: If the queue is empty, add the new node as the head */
    if (queue_empty(queue))
    {
        queue->head = new_node;
        return;
    }

    /* Traverse the queue to find the correct insertion point */
    while (current != NULL)
    {
        int current_floor_number = floor_to_int(current->data.floor);

        /* Check for a duplicate node in the same direction that hasn’t been
         * displayed yet */
        if (current->data.direction == direction &&
            current_floor_number == floor_number &&
            !current->data.been_displayed)
        {
            node_deinit(
                &new_node); // Free the new node and do not add it to the queue
            return;
        }

        /*
         * Determine if the new node should be inserted:
         * - For 'up' direction nodes, insert if the new floor is below the
         * current floor.
         */
        bool should_insert_up = direction == UP_FLOOR &&
                                current->data.direction == UP_FLOOR &&
                                floor_number < current_floor_number;

        /*
         * - For 'down' direction nodes, insert if the new floor is above the
         * current floor.
         */
        bool should_insert_down = direction == DOWN_FLOOR &&
                                  current->data.direction == DOWN_FLOOR &&
                                  floor_number > current_floor_number;

        /*
         * - If at a boundary between different direction blocks, insert at the
         * end of the current block.
         */
        bool should_insert_boundary = current->data.direction != direction &&
                                      prev && prev->data.direction == direction;

        /* Insert the new node if one of the conditions is met */
        if (should_insert_up || should_insert_down || should_insert_boundary)
        {
            new_node->next = current;
            if (prev == NULL)
            {
                queue->head = new_node; // Insert at the head
            }
            else
            {
                prev->next = new_node; // Insert between prev and current
            }
            return;
        }

        /* Move to the next node in the queue */
        prev = current;
        current = current->next;
    }

    /* If reached the end, add the new node at the end of the queue */
    if (prev)
    {
        prev->next = new_node;
    }
    else
    {
        queue->head =
            new_node; // This case should not occur due to earlier checks
    }
}

/*
 * Removes the head node from the queue.
 */
void dequeue(queue_t *queue)
{
    if (queue->head == NULL)
        return;
    node_t *head = queue->head;
    queue->head = head->next; // Update head to the next node
    node_deinit(&head);       // Free the original head
}

/*
 * Prints all nodes in the queue with their floor and direction.
 * This function is useful for debugging purposes.
 */
void print_queue(queue_t *queue)
{
    node_t *current = queue->head;
    while (current != NULL)
    {
        const char *direction = current->data.direction == UP_FLOOR ? "U" : "D";
        if (current->data.been_displayed)
        {
            printf("(%s%s) ", direction,
                   current->data
                       .floor); // Displayed nodes are enclosed in brackets
        }
        else
        {
            printf(
                "%s%s ", direction,
                current->data.floor); // Undisplayed nodes are printed plainly
        }
        current = current->next;
    }
    printf("\n");
}

/*
 * Adds a source and destination floor as a pair to the queue in the correct
 * direction.
 */
void enqueue_pair(queue_t *queue, const char *source_floor,
                  const char *destination_floor)
{
    int source_number = floor_to_int(source_floor);
    int destination_number = floor_to_int(destination_floor);
    floor_direction_t direction =
        source_number > destination_number ? DOWN_FLOOR : UP_FLOOR;

    enqueue(queue, source_floor, direction);      // Enqueue source floor
    enqueue(queue, destination_floor, direction); // Enqueue destination floor
}

/*
 * Finds and returns the first floor in the queue that has not been displayed
 * yet, marking it as displayed upon return.
 */
char *queue_get_undisplayed(queue_t *queue)
{
    if (queue_empty(queue))
        return NULL;
    else
    {
        node_t *current = queue->head;
        /* Loop until an undisplayed node is found or the end is reached */
        while (current->data.been_displayed && current->next != NULL)
            current = current->next;
        if (current->data.been_displayed)
            return NULL;
        /* Mark the current node as been displayed */
        current->data.been_displayed = true;
        return current->data.floor;
    }
}

/*
 * Checks if the queue is empty.
 */
bool queue_empty(const queue_t *queue) { return queue->head == NULL; }

/*
 * Returns the floor of the most recent displayed node, stopping at the first
 * match.
 */
char *queue_prev_floor(queue_t *queue)
{
    if (queue_empty(queue))
        return NULL;
    else
    {
        node_t *current = queue->head;
        while (current->next != NULL && current->next->data.been_displayed)
        {
            current = current->next;
        }
        return current->data.floor;
    }
}
