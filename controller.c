#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Single-threaded TCP/IP server for managing the elevator systems multiple
 * car connections. It facilitates communication between cars and call pads.
 * The server uses signal handling for graceful termination and the
 * select() system call to efficiently manage incoming messages from clients and
 * elevator cars.
 *
 * Perhaps a multi-threaded implementation could be more effective but the
 * specification requirements and the limited number of elevator shafts in
 * typical buildings make a single-threaded option robust and maintainable while
 * still being efficient.
 */

#include "controller.h"
#include "global.h"
#include "queue.h"
#include "tcpip.h"

static volatile sig_atomic_t keep_running = 1;

/*
 * Signal handler function for handling SIGINT signal
 */
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

    /* Register signal handler for SIGINT */
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    controller_t controller;
    controller_init(&controller);

    /* Main loop that continuously processes incoming messages until a
     * termination signal is received. */
    while (keep_running)
    {
        handle_incoming_messages(&controller);
    }

    /* Deinitialise controller to free up resources */
    controller_deinit(&controller);

    return 0;
}

/*
 * Initializes a car connection structure with default values, including setting
 * the socket descriptor to zero and initializing the message queue.
 */
void car_connection_init(car_connection_t *c)
{
    c->sd = 0;
    c->name = NULL;
    c->lowest_floor = NULL;
    c->highest_floor = NULL;
    queue_init(&c->queue);
}

/*
 * Cleans up the car connection by closing the socket, freeing allocated memory
 * for the car name and floor details, and deinitializing the message queue.
 */
void car_connection_deinit(car_connection_t *car_connection)
{
    close(car_connection->sd);
    car_connection->sd = -1;

    free(car_connection->name);
    free(car_connection->lowest_floor);
    free(car_connection->highest_floor);

    queue_deinit(&car_connection->queue);
}

/*
 * Initializes the controller by setting up the server socket and preparing the
 * car connection structures for use.
 */
void controller_init(controller_t *controller)
{
    server_init(&controller->server_sd, &controller->sock);

    controller->num_car_connections = 0;
    for (int i = 0; i < BACKLOG; i++)
    {
        car_connection_init(&controller->car_connections[i]);
    }
}

/*
 * Deinitializes the controller, ensuring all resources are released and closing
 * any active car connections.
 */
void controller_deinit(controller_t *controller)
{
    close(controller->server_sd);
    controller->server_sd = -1;

    /* Deinitialize each car connection if it is set */
    for (size_t i = 0; i < BACKLOG; i++)
    {
        car_connection_t *c = &controller->car_connections[i];
        if (FD_ISSET(c->sd, &controller->readfds))
        {
            car_connection_deinit(c);
        }
    }
}

/*
 * Handles an incoming call request to the elevator system by finding a suitable
 * car and managing the request queue.
 */
void handle_call(controller_t *controller, int sd, const char *source_floor,
                 const char *destination_floor)
{
    /*
     * Find a car capable of servicing the call and handle it.
     */
    for (size_t i = 0; i < controller->num_car_connections; i++)
    {
        car_connection_t *c = &controller->car_connections[i];

        /* Check if the source and destination floors are within the car's range
         */
        if (floor_in_range(source_floor, c->lowest_floor, c->highest_floor) ==
                0 &&
            floor_in_range(destination_floor, c->lowest_floor,
                           c->highest_floor) == 0)
        {
            /* Add source and destination floor to the queue */
            enqueue_pair(&c->queue, source_floor, destination_floor);

            send_message(sd, "CAR %s", c->name);
            /* Get the next undisplayed floor and send a message to the car */
            char *next_floor = queue_get_undisplayed(&c->queue);
            if (next_floor != NULL)
            {
                send_message(c->sd, "FLOOR %s", next_floor);
            }
            else
            {
                printf("Something went wrong with the car scheduling\n");
            }

            return;
        }
    }

    /* If no car wa found. */
    send_message(sd, "UNAVAILABLE");
}

/*
 * Adds a new car connection to the controller
 */
void add_car_connection(controller_t *controller, int sd, const char *name,
                        const char *lowest_floor, const char *highest_floor)
{
    car_connection_t new_car_connection = {
        sd, strdup(name), strdup(lowest_floor), strdup(highest_floor), {NULL}};
    controller->car_connections[controller->num_car_connections] =
        new_car_connection;
    controller->num_car_connections += 1;
}

/*
 * Handles messages received from the server, distinguishing between car call
 * requests and new car connection messages.
 */
void handle_server_message(controller_t *controller, char *message,
                           int client_sock)
{
    char *saveptr;
    const char *connection_type = strtok_r(message, " ", &saveptr);

    /* Check to see if the incoming message is a call for a car or a new car
     * connection. */
    if (strcmp(connection_type, "CALL") == 0)
    {
        /* extract the source and destination floors from the call message and
         * handle the call. */
        const char *source_floor = strtok_r(NULL, " ", &saveptr);
        const char *destination_floor = strtok_r(NULL, " ", &saveptr);

        handle_call(controller, client_sock, source_floor, destination_floor);
    }
    else if (strcmp(connection_type, "CAR") == 0)
    {
        /* Extract the name, lowest and highest floors from the message and add
         * the new car connection */
        const char *name = strtok_r(NULL, " ", &saveptr);
        const char *lowest_floor = strtok_r(NULL, " ", &saveptr);
        const char *highest_floor = strtok_r(NULL, "", &saveptr);

        add_car_connection(controller, client_sock, name, lowest_floor,
                           highest_floor);
    }
}

/*
 * Handles messages received from car connections
 */
void handle_car_connection_message(controller_t *controller,
                                   car_connection_t *c, char *message)
{
    char *saveptr;

    /*
     * Checks if the car is in emergency or individual service mode, removes it
     * from the FD_SET, and deinitializes the connection.
     */
    if (strcmp(message, "EMERGENCY") == 0 ||
        strcmp(message, "INDIVIDUAL SERVICE") == 0)
    {
        FD_CLR(c->sd, &controller->readfds);
        car_connection_deinit(c);
        shift_car_connections(controller);
    }
    else if (strcmp(strtok_r(message, " ", &saveptr), "STATUS") == 0)
    {
        /*
         * Otherwise the controller recieved a status update and should decide
         * weather it should schedule the car ferther.
         */
        const char *status = strtok_r(NULL, " ", &saveptr);
        const char *current_floor = strtok_r(NULL, " ", &saveptr);

        schedule_car(c, status, current_floor);
    }
}

/*
 * Handles incoming messages from clients by managing the socket descriptors and
 * processing new connections and existing messages.
 */
void handle_incoming_messages(controller_t *controller)
{
    /*
     * The FD_SET must be rebuilt on each iteration of the loop because the
     * select() function modifies the set to reflect which file descriptors have
     * activity. Therefore, we initialize it at the beginning of each loop
     * iteration to ensure it accurately represents the current state of socket
     * connections.
     */

    /* Clear the FD_SET */
    FD_ZERO(&controller->readfds);
    FD_SET(controller->server_sd, &controller->readfds);
    controller->max_sd = controller->server_sd;

    /* Add any current clients back into the FD_SET */
    for (int i = 0; i < BACKLOG; i++)
    {
        car_connection_t car_connection = controller->car_connections[i];
        if (car_connection.sd == -1)
            continue;
        else if (car_connection.sd > 0)
            FD_SET(car_connection.sd, &controller->readfds);
        if (car_connection.sd > controller->max_sd)
            controller->max_sd = car_connection.sd;
    }

    /* Wait for activity on any of the sockets */
    int activity =
        select(controller->max_sd + 1, &controller->readfds, NULL, NULL, NULL);
    if (activity < 0 && errno != EINTR)
    {
        perror("Select error");
        exit(EXIT_FAILURE);
    }

    /* Check to see if the SIGINT signal was sent while waiting for activity */
    if (!keep_running)
        return;

    /* Handle incoming connection requests */
    if (FD_ISSET(controller->server_sd, &controller->readfds))
    {
        /* Accept a new connection */
        int client_sock = accept(controller->server_sd, NULL, NULL);
        if (client_sock < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            perror("read()");
            return;
        }

        /* Handle incoming messages from new connections */
        char *message = receive_msg(client_sock);
        handle_server_message(controller, message, client_sock);
        free(message);
    }

    /* Handle incoming messages from car connections */
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

/*
 * Schedules the car based on its status and current floor
 */
void schedule_car(car_connection_t *c, const char *status,
                  const char *current_floor)
{
    /*
     * Schedules the car for the next FLOOR message if the doors are opening,
     * the queue is not empty, and the current floor matches the last FLOOR
     * message.
     */
    if (strcmp(status, "Opening") == 0 &&
        strcmp(queue_prev_floor(&c->queue), current_floor) == 0 &&
        !queue_empty(&c->queue))
    {
        char *next_floor = queue_get_undisplayed(&c->queue);
        if (next_floor != NULL)
        {
            send_message(c->sd, "FLOOR %s", next_floor);
        }
    }
}

void shift_car_connections(controller_t *controller)
{
    for (int i = 0; i < controller->num_car_connections; i++)
    {
        const car_connection_t *c = &controller->car_connections[i];

        if (c->sd == -1)
        {
            // Shift all subsequent elements up
            for (int j = i; j < controller->num_car_connections - 1; j++)
            {
                controller->car_connections[j] =
                    controller->car_connections[j + 1];
            }
            controller->num_car_connections -= 1; // Decrease the count
            i -= 1; // Stay on the same index for the next iteration
        }
    }
}
