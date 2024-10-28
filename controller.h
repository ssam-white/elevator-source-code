#pragma once

#include <arpa/inet.h>

#include "queue.h"
#include "tcpip.h"

typedef struct car_connection {
	int sd;
	char *name;
	char *lowest_floor;
	char *highest_floor;
	queue_t queue;
} car_connection_t;

typedef struct {
	int server_fd;
	struct sockaddr_in sock;
    fd_set readfds;
	int max_sd;
	size_t num_car_connections;
	car_connection_t car_connections[BACKLOG];
} controller_t;

void car_connection_init(car_connection_t *);

void controller_init(controller_t *);
void car_connection_deinit(car_connection_t *);
void controller_deinit(controller_t *);
void server_init(int *, struct sockaddr_in *);
void handle_call(controller_t *, int, char *, char *);
void add_car_connection(controller_t *, int, char *, char *, char *);
void handle_server_message(controller_t *, char *, int);
void handle_car_connection_message(controller_t *, car_connection_t *, char *);
