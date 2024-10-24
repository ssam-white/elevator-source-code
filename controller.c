#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "tcpip.h"
#include "controller.h"

int main(void)
{
	controller_t controller;
	controller_init(&controller);

	while (1) {
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

		if (FD_ISSET(controller.server_fd, &controller.readfds)) {
			int client_sock = accept(controller.server_fd, NULL, NULL);
			char *message = receive_msg(client_sock);

			char *connection_type = strtok(message, " ");
			if (strcmp(connection_type, "CALL") == 0) {
				char *source_floor = strtok(NULL, " ");
				char *destination_floor = strtok(NULL, " ");

				handle_call(&controller, client_sock, source_floor, destination_floor);
			} else if (strcmp(connection_type, "CAR") == 0) {
				char *name = strtok(NULL, " ");
				char *lowest_floor = strtok(NULL, " ");
				char *highest_floor = strtok(NULL, " ");

				add_car_connection(&controller, client_sock, name, lowest_floor, highest_floor);
			}
		}
	}

	return 0;
}

void controller_init(controller_t *controller) 
{
	server_init(&controller->server_fd, &controller->sock);
	controller->num_car_connections = 0;

	car_connection_t d;
	d.sd = 0;
	d.name = NULL;
	
	controller->num_car_connections = 0;
	for (int i = 0; i < BACKLOG; i++) {
		controller->car_connections[i] = d;
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

int accept_new_connection(controller_t *controller) {
	socklen_t client_len = sizeof(controller->client_addr);

	controller->client_fd = accept(controller->server_fd, (struct sockaddr *) &controller->client_addr, &client_len);
	if (controller->client_fd == -1) {
		perror("accept()");
		return -1;
	}
	return 0;
}

void handle_call(controller_t *controller, int sd, char *source_floor, char *destination_floor)
{
	if (controller->num_car_connections == 0) {
		send_message(sd, "UNAVAILABLE");
	} else {
		car_connection_t selected_car = controller->car_connections[0];
		char *response = malloc(strlen(selected_car.name) + 4);
		sprintf(response, "CAR %s", selected_car.name);
		send_message(sd, response);
		free(response);
	}

}
void add_car_connection(controller_t *controller, int sd, char *name, char *lowest_floor, char *highest_floor) 
{
	car_connection_t new_car_connection = { sd, strdup(name), strdup(lowest_floor), strdup(highest_floor) };
	controller->car_connections[controller->num_car_connections] = new_car_connection;
	controller->num_car_connections += 1;

}
