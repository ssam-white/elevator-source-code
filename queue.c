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

void node_init(node_t **node, const char *floor, floor_direction_t direction,
               node_t *next)
{
    *node = (node_t *)calloc(1, sizeof(node_t));

    (*node)->data.floor = strdup(floor);

    (*node)->data.direction = direction;
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

node_t *get_last(queue_t *queue)
{
    if (queue->head == NULL)
        return NULL;
    node_t *current_node = queue->head;
    while (current_node->next != NULL)
    {
        current_node = current_node->next;
    }
    return current_node;
}

void enqueue(queue_t *queue, const char *floor, floor_direction_t direction)
{
    if (queue->head == NULL)
    {
        node_t *new_node;
        node_init(&new_node, floor, direction, NULL);
        queue->head = new_node;
    }
    else
    {
        node_t *current_node = queue->head;
        while (current_node->next != NULL)
        {
            if (strcmp(current_node->data.floor, floor) == 0 &&
                current_node->data.direction == direction)
                return;
            current_node = current_node->next;
        }
        node_t *new_node;
        node_init(&new_node, floor, direction, NULL);
        current_node->next = new_node;
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
        printf("%s%s ", direction, current->data.floor);
        current = current->next;
    }
    printf("\n");
}

void enqueue_pair(queue_t *queue, const char *source_floor,
                  const char *destination_floor)
{
    int source_number = floor_to_int(source_floor);
    int destination_number = floor_to_int(destination_floor);
    floor_direction_t direction =
        source_number > destination_number ? DOWN_FLOOR : UP_FLOOR;

    enqueue(queue, source_floor, direction);
    enqueue(queue, destination_floor, direction);
}

char *queue_peek_current(queue_t *queue)
{
    node_t *current = queue_get_current(queue);
    return current == NULL ? NULL : current->data.floor;
}

void queue_set_between(queue_t *queue, bool value) { queue->between = value; }

node_t *queue_get_current(queue_t *queue)
{
    if (queue->head == NULL)
        return NULL;
    return queue->between ? queue->head->next : queue->head;
}
