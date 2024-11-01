#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "queue.h"

void queue_init(queue_t *queue)
{
    queue->head = NULL;
    queue->between = false;
}

void queue_deinit(queue_t *queue)
{
    node_t *current = queue->head;
    node_t *next;
    while (current != NULL)
    {
        next = current->next;
        node_deinit(&current);
        current = next;
    }
    queue->head = NULL;
}

void node_init(node_t **node, const char *floor, floor_direction_t direction, node_t *next)
{
    *node = (node_t *)calloc(1, sizeof(node_t));

    (*node)->data.floor = strdup(floor);

    (*node)->data.direction = direction;
    (*node)->data.been_displayed = false;
    (*node)->next = next;
}

void node_deinit(node_t **node)
{
    if (node && *node)
    {
        free((*node)->data.floor);
        free(*node);
        *node = NULL;
    }
}

void enqueue(queue_t *queue, const char *floor, floor_direction_t direction)
{
    node_t *current = queue->head;
    node_t *prev = NULL;
    node_t *new_node = NULL;
    node_init(&new_node, floor, direction, NULL);

    int floor_number = floor_to_int(floor);

    // Edge case: If the queue is empty, add the new node as the head
    if (queue->head == NULL)
    {
        queue->head = new_node;
        return;
    }

    // Traverse the queue to find the correct insertion point
    while (current != NULL)
    {
        int current_floor_number = floor_to_int(current->data.floor);

        // Check if there's an existing node with the same floor and direction that hasn't been
        // displayed
        if (current->data.direction == direction && current_floor_number == floor_number &&
            current->data.been_displayed == false)
        {
            // Duplicate found, so no need to insert
            return;
        }

        // In the UP block, maintain ascending order
        if (direction == UP_FLOOR && current->data.direction == UP_FLOOR)
        {
            if (floor_number < current_floor_number)
            {
                // Insert the new node before the current node
                new_node->next = current;
                if (prev == NULL)
                {
                    queue->head = new_node;
                }
                else
                {
                    prev->next = new_node;
                }
                return;
            }
        }
        // In the DOWN block, maintain descending order
        else if (direction == DOWN_FLOOR && current->data.direction == DOWN_FLOOR)
        {
            if (floor_number > current_floor_number)
            {
                // Insert the new node before the current node
                new_node->next = current;
                if (prev == NULL)
                {
                    queue->head = new_node;
                }
                else
                {
                    prev->next = new_node;
                }
                return;
            }
        }
        // If we hit the boundary between UP and DOWN, insert at this boundary
        else if (current->data.direction != direction && prev && prev->data.direction == direction)
        {
            new_node->next = current;
            prev->next = new_node;
            return;
        }

        // Move to the next node
        prev = current;
        current = current->next;
    }

    // If we reached the end, add the new node at the end
    if (prev)
    {
        prev->next = new_node;
    }
    else
    {
        queue->head = new_node;
    }
}

void dequeue(queue_t *queue)
{
    if (queue->head == NULL)
        return;
    node_t *head = queue->head;
    queue->head = head->next;
    node_deinit(&head);
}

void print_queue(queue_t *queue)
{
    node_t *current = queue->head;
    while (current != NULL)
    {
        const char *direction = current->data.direction == UP_FLOOR ? "U" : "D";
        if (current->data.been_displayed)
        {
            printf("(%s%s) ", direction, current->data.floor);
        }
        else
        {
            printf("%s%s ", direction, current->data.floor);
        }
        current = current->next;
    }
    printf("\n");
}

void enqueue_pair(queue_t *queue, const char *source_floor, const char *destination_floor)
{
    int source_number = floor_to_int(source_floor);
    int destination_number = floor_to_int(destination_floor);
    floor_direction_t direction = source_number > destination_number ? DOWN_FLOOR : UP_FLOOR;

    enqueue(queue, source_floor, direction);
    enqueue(queue, destination_floor, direction);
}

char *queue_peek(queue_t *queue)
{
    if (queue_empty(queue))
        return NULL;
    else
    {
        return queue->head->data.floor;
    }
}

char *queue_get_undisplayed(queue_t *queue)
{
    if (queue_empty(queue))
        return NULL;
    else
    {
        node_t *current = queue->head;
        while (current->data.been_displayed && current->next != NULL)
            current = current->next;
        if (current->data.been_displayed)
            return NULL;
        current->data.been_displayed = true;
        return current->data.floor;
    }
}

bool queue_empty(queue_t *queue) { return queue->head == NULL; }


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
        current->data.been_displayed = true;
        return current->data.floor;
    }
}
