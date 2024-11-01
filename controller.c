#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "controller.h"
#include "global.h"
#include "queue.h"
#include "tcpip.h"

static volatile sig_atomic_t keep_running = 1;

static void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        keep_running = 0;
    }
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    controller_t controller;
    controller_init(&controller);

    while (keep_running)
    {
        handle_incoming_messages(&controller);
    }

    controller_deinit(&controller);

    return 0;
}

void car_connection_init(car_connection_t *c)
{
    c->sd = 0;
    c->name = NULL;
    c->lowest_floor = NULL;
    c->highest_floor = NULL;
    queue_init(&c->queue);
}

void car_connection_deinit(car_connection_t *car_connection)
{
    close(car_connection->sd);
    car_connection->sd = -1;

    free(car_connection->name);
    free(car_connection->lowest_floor);
    free(car_connection->highest_floor);

    queue_deinit(&car_connection->queue);
}

void controller_init(controller_t *controller)
{
    server_init(&controller->server_fd, &controller->sock);

    controller->num_car_connections = 0;
    for (int i = 0; i < BACKLOG; i++)
    {
        car_connection_init(&controller->car_connections[i]);
    }
}

void controller_deinit(controller_t *controller)
{
    close(controller->server_fd);
    controller->server_fd = -1;

    for (size_t i = 0; i < BACKLOG; i++)
    {
        car_connection_t *c = &controller->car_connections[i];
        if (FD_ISSET(c->sd, &controller->readfds))
        {
            car_connection_deinit(c);
        }
    }
}

void server_init(int *fd, struct sockaddr_in *sock)
{
    memset(sock, 0, sizeof(*sock));
    sock->sin_family = AF_INET;
    sock->sin_port = htons(3000);
    sock->sin_addr.s_addr = htonl(INADDR_ANY);

    // Create the socket
    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd == -1)
    {
        perror("socket()");
        exit(1);
    }

    // Set socket options to allow reuse of address
    int opt_enable = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1)
    {
        perror("setsockopt()");
        close(*fd); // Close the socket before exiting
        exit(1);
    }

    // Bind the socket to the address and port
    if (bind(*fd, (const struct sockaddr *)sock, sizeof(*sock)) == -1)
    {
        perror("bind()");
        close(*fd); // Close the socket before exiting
        exit(1);
    }

    // Start listening for incoming connections
    if (listen(*fd, 10) == -1)
    {
        perror("listen()");
        close(*fd); // Close the socket before exiting
        exit(1);
    }
}

void handle_call(controller_t *controller, int sd, const char *source_floor,
                 const char *destination_floor)
{
    for (size_t i = 0; i < controller->num_car_connections; i++)
    {
        car_connection_t *c = &controller->car_connections[i];

        if (floor_in_range(source_floor, c->lowest_floor, c->highest_floor) == 0 &&
            floor_in_range(destination_floor, c->lowest_floor, c->highest_floor) == 0)
        {
            enqueue_pair(&c->queue, source_floor, destination_floor);

            send_message(sd, "CAR %s", c->name);
            char *next_floor = queue_get_undisplayed(&c->queue);
            if (next_floor != NULL)
            {
                send_message(c->sd, "FLOOR %s", next_floor);
            }
            else
            {
                printf("Somthing went wrong with the car scheduling\n");
            }

            return;
        }
    }

    send_message(sd, "UNAVAILABLE");
}

void add_car_connection(controller_t *controller, int sd, const char *name,
                        const char *lowest_floor, const char *highest_floor)
{
    car_connection_t new_car_connection = {sd, strdup(name), strdup(lowest_floor),
                                           strdup(highest_floor), NULL};
    controller->car_connections[controller->num_car_connections] = new_car_connection;
    controller->num_car_connections += 1;
}

void handle_server_message(controller_t *controller, char *message, int client_sock)
{
    char *saveptr;
    const char *connection_type = strtok_r(message, " ", &saveptr);

    if (strcmp(connection_type, "CALL") == 0)
    {
        const char *source_floor = strtok_r(NULL, " ", &saveptr);
        const char *destination_floor = strtok_r(NULL, " ", &saveptr);

        handle_call(controller, client_sock, source_floor, destination_floor);
    }
    else if (strcmp(connection_type, "CAR") == 0)
    {
        const char *name = strtok_r(NULL, " ", &saveptr);
        const char *lowest_floor = strtok_r(NULL, " ", &saveptr);
        const char *highest_floor = strtok_r(NULL, "  ", &saveptr);

        add_car_connection(controller, client_sock, name, lowest_floor, highest_floor);
    }
}

void handle_car_connection_message(controller_t *controller, car_connection_t *c, char *message)
{
    char *saveptr;

    if (strcmp(message, "EMERGENCY") == 0 || strcmp(message, "INDIVIDUAL SERVICE") == 0)
    {
        FD_CLR(c->sd, &controller->readfds);
        car_connection_deinit(c);
		controller->num_car_connections -= 1;
    }
    else if (strcmp(strtok_r(message, " ", &saveptr), "STATUS") == 0)
    {
        const char *status = strtok_r(NULL, " ", &saveptr);
        const char *current_floor = strtok_r(NULL, " ", &saveptr);
        const char *destination_floor = strtok_r(NULL, " ", &saveptr);

        schedule_car(c, status, current_floor, destination_floor);
    }
}

void handle_incoming_messages(controller_t *controller)
{
    FD_ZERO(&controller->readfds);
    FD_SET(controller->server_fd, &controller->readfds);
    controller->max_sd = controller->server_fd;

    for (int i = 0; i < BACKLOG; i++)
    {
        car_connection_t car_connection = controller->car_connections[i];
        if (car_connection.sd > 0)
            FD_SET(car_connection.sd, &controller->readfds);
        if (car_connection.sd > controller->max_sd)
            controller->max_sd = car_connection.sd;
    }

    int activity = select(controller->max_sd + 1, &controller->readfds, NULL, NULL, NULL);
    if (activity < 0 && errno != EINTR)
    {
        perror("Select error");
        exit(EXIT_FAILURE);
    }

    if (!keep_running)
        return;

    if (FD_ISSET(controller->server_fd, &controller->readfds))
    {
        int client_sock = accept(controller->server_fd, NULL, NULL);
        if (client_sock < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            perror("read()");
            return;
        }

        char *message = receive_msg(client_sock);
        handle_server_message(controller, message, client_sock);
        free(message);
    }

    for (int i = 0; i < BACKLOG; i++)
    {
        car_connection_t *c = &controller->car_connections[i];
        if (FD_ISSET(c->sd, &controller->readfds))
        {
            char *message = receive_msg(c->sd);
            handle_car_connection_message(controller, c, message);
            free(message);
        }
    }
}

void schedule_car(car_connection_t *c, const char *status, const char *current_floor,
                  const char *destination_floor)
{
    // char *next_floor = queue_get_undisplayed(&c->queue);
    //
    if (strcmp(status, "Opening") == 0 && strcmp(queue_prev_floor(&c->queue), current_floor) == 0 &&
        !queue_empty(&c->queue))
    {
        char *next_floor = queue_get_undisplayed(&c->queue);
        if (next_floor != NULL)
        {
            send_message(c->sd, "FLOOR %s", next_floor);
        }
    }
}

void dequeue_visited_floors(car_connection_t *c, const char *status, const char *current_floor,
                            const char *destination_floor)
{
    if (strcmp(status, "Opening") == 0 && !queue_empty(&c->queue) &&
        strcmp(queue_peek(&c->queue), current_floor) == 0)
    {
        dequeue(&c->queue);
    }
    else if (strcmp(status, "Between") == 0 && !queue_empty(&c->queue))
    {
        int current_floor_number = floor_to_int(current_floor);
        int destination_floor_number = floor_to_int(destination_floor);
        int difference = current_floor_number - destination_floor_number;

        if (difference == 1 || difference == -1)
        {
            dequeue(&c->queue);
        }
    }
}
