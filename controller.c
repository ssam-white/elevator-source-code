#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "tcpip.h"
#include "controller.h"

int main(void)
{
	sigset_t set;
	int sig;
	// initialize the signal set
	sigemptyset(&set);
	// add SIGINT to the set of signals
	sigaddset(&set, SIGINT);
	// block the defult behavour of SIGINT
	sigprocmask(SIG_BLOCK, &set, NULL);

	controller_t controller;
	controller_init(&controller);


	while (1) {
		if (accept_new_connection(&controller) < 0) {
			return 1;
		}

		char *message = receive_msg(controller.client_fd);
		if (strstr(message, "CALL")) {
			send_message(controller.client_fd, "UNAVAILABLE");
		} else if (strstr(message, "CAR")) {
			add_client_ad_car_connection(&controller);
		}
	}


	sigwait(&set, &sig);

	return 0;
}

void controller_init(controller_t *controller) 
{
	server_init(&controller->server_fd, &controller->sock);
	controller->num_car_connections = 0;
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
}

void add_client_ad_car_connection(controller_t *controller) {
	car_connection_t car_connection;
	car_connection.car_fd = controller->client_fd;
	car_connection.car_addr = controller->client_addr;

	if (controller->car_connections == NULL) {
		controller->car_connections = (car_connection_t *) calloc(1, sizeof(car_connection_t));
		controller->car_connections[0] = car_connection;
		controller->num_car_connections = 1;
	}
}
