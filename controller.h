#pragma once

#include <arpa/inet.h>

typedef struct car_connection {
	int car_fd;
	struct sockaddr_in car_addr;
} car_connection_t;

typedef struct {
	int server_fd;
	struct sockaddr_in sock;
	int client_fd;
	struct sockaddr_in  client_addr;
	size_t num_car_connections;
	car_connection_t *car_connections;
} controller_t;

void controller_init(controller_t *);
void server_init(int *, struct sockaddr_in *);
int accept_new_connection(controller_t *);
void add_client_ad_car_connection(controller_t *);
