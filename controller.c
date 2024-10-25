#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#include "queue.h"
#include "global.h"
#include "tcpip.h"
#include "controller.h"

static volatile sig_atomic_t keep_running = 1;

static void signal_handler(int signum) {
	switch (signum) {
		case SIGINT:
			keep_running = 0;
			break;
		default:
			break;
	}
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

	controller_t controller;
	controller_init(&controller);

	while (keep_running) {
		FD_ZERO(&controller.readfds);	
		FD_SET(controller.server_fd, &controller.readfds);
		controller.max_sd = controller.server_fd;

		for (int i = 0; i < BACKLOG; i++) {
			car_connection_t car_connection = controller.car_connections[i];
			if (car_connection.sd > 0) FD_SET(car_connection.sd, &controller.readfds);
			if (car_connection.sd > controller.max_sd) controller.max_sd = car_connection.sd;
		}

		int activity = select(controller.max_sd + 1, &controller.readfds, NULL, NULL, NULL);
		if (activity < 0 && errno != EINTR) {
			perror("Select error");
			exit(EXIT_FAILURE);
		}

		if (!keep_running) break;

		if (FD_ISSET(controller.server_fd, &controller.readfds)) {
			int client_sock = accept(controller.server_fd, NULL, NULL);
			if (client_sock < 0) {
				if (errno == EINTR) {
					break;
				}
				perror("read()");
				return 1;
			}

			char *message = receive_msg(client_sock);
			handle_server_message(&controller, message, client_sock);
			free(message);
		}

		for (int i = 0; i < BACKLOG; i++) {
			car_connection_t *c = &controller.car_connections[i];
			if (FD_ISSET(c->sd, &controller.readfds)) {
				char *message = receive_msg(c->sd);
				handle_car_connection_message(&controller, c, message);
				free(message);
			}
		}
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
	c->destination_floor = NULL;
	queue_init(&c->queue);
}

void car_connection_deinit(car_connection_t *car_connection)
{
	close(car_connection->sd);
	car_connection->sd = -1;

	free(car_connection->name);
	free(car_connection->lowest_floor);
	free(car_connection->highest_floor);
	free(car_connection->destination_floor);

	queue_deinit(&car_connection->queue);
}

void controller_init(controller_t *controller) 
{
	server_init(&controller->server_fd, &controller->sock);

	controller->num_car_connections = 0;
	for (int i = 0; i < BACKLOG; i++) {
		car_connection_init(&controller->car_connections[i]);
	}
}

void controller_deinit(controller_t *controller) 
{
	close(controller->server_fd);
	controller->server_fd = -1; 

	for (size_t i = 0; i < BACKLOG; i++) {
		car_connection_t *c = &controller->car_connections[i];
		if (FD_ISSET(c->sd, &controller->readfds)) {
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
    if (*fd == -1) {
        perror("socket()");
        exit(1);
    }

    // Set socket options to allow reuse of address
    int opt_enable = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1) {
        perror("setsockopt()");
        close(*fd);  // Close the socket before exiting
        exit(1);
    }

    // Bind the socket to the address and port
    if (bind(*fd, (const struct sockaddr *)sock, sizeof(*sock)) == -1) {
        perror("bind()");
        close(*fd);  // Close the socket before exiting
        exit(1);
    }

    // Start listening for incoming connections
    if (listen(*fd, 10) == -1) {
        perror("listen()");
        close(*fd);  // Close the socket before exiting
        exit(1);
    }
}

void handle_call(controller_t *controller, int sd, char *source_floor, char *destination_floor)
{
	for (size_t i = 0; i < controller->num_car_connections; i++) {
		car_connection_t *c = &controller->car_connections[i];

		if (
			floor_in_range(source_floor, c->lowest_floor, c->highest_floor) == 0 &&
			floor_in_range(destination_floor, c->lowest_floor, c->highest_floor) == 0
		) {
			enqueue(&c->queue, source_floor, DOWN_FLOOR);
			enqueue(&c->queue, destination_floor, DOWN_FLOOR);

			char *f = dequeue(&c->queue);

			send_message(sd, "CAR %s", c->name);
			send_message(c->sd, "FLOOR %s", f);
			free(f);
			return;
		}
	}

	send_message(sd, "UNAVAILABLE");
}

void add_car_connection(controller_t *controller, int sd, char *name, char *lowest_floor, char *highest_floor) 
{
	car_connection_t new_car_connection = { sd, strdup(name), strdup(lowest_floor), strdup(highest_floor), strdup(lowest_floor), NULL};
	controller->car_connections[controller->num_car_connections] = new_car_connection;
	controller->num_car_connections += 1;
}

void handle_server_message(controller_t *controller, char *message, int client_sock) 
{
	char *connection_type = strtok(message, " ");
	if (strcmp(connection_type, "CALL") == 0) {
		char *source_floor = strtok(NULL, " ");
		char *destination_floor = strtok(NULL, " ");

		handle_call(controller, client_sock, source_floor, destination_floor);
	} else if (strcmp(connection_type, "CAR") == 0) {
		char *name = strtok(NULL, " ");
		char *lowest_floor = strtok(NULL, " ");
		char *highest_floor = strtok(NULL, "  ");

		add_car_connection(controller, client_sock, name, lowest_floor, highest_floor);
	}

}

void handle_car_connection_message(controller_t *controller, car_connection_t *c, char *message) 
{
	if (
		strcmp(message, "EMERGENCY") == 0 ||
		strcmp(message, "INDIVIDUAL SERVICE") == 0
	) {
		FD_CLR(c->sd, &controller->readfds);
		car_connection_deinit(c);
	} else if (strcmp(strtok(message, " "), "STATUS") == 0) {
		char *status = strtok(NULL, " ");
		char *current_floor = strdup(strtok(NULL, " "));
		char *destination_floor = strtok(NULL, " ");

		if (
			strcmp(status, "Opening") == 0 &&
			c->queue.head != NULL
		) {
			char *f = dequeue(&c->queue);
			send_message(c->sd, "FLOOR %s", f);
			free(f);
		}
	}

}
