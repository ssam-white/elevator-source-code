#pragma once

#include <arpa/inet.h>

#include "tcpip.h"

typedef struct car_connection {
	int sd;
	char *name;
	char *lowest_floor;
	char *highest_floor;
} car_connection_t;

typedef struct {
	int server_fd;
	struct sockaddr_in sock;
	int client_fd;
	struct sockaddr_in  client_addr;
    fd_set readfds;
	int max_sd;
	size_t num_car_connections;
	car_connection_t car_connections[BACKLOG];
} controller_t;

void controller_init(controller_t *);
void server_init(int *, struct sockaddr_in *);
int accept_new_connection(controller_t *);
void handle_call(controller_t *, int, char *, char *);
void add_car_connection(controller_t *, int, char *, char *, char *);
