#pragma once

#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

typedef enum {
	SUCCESS = 0,
	CALL_CONNECTION_ERROR = -1,
	CALL_SOCK_CREATION_ERROR = -2,
	CALL_INVALID_ADDRESS_ERROR = -3,
} call_pad_error_t;

typedef struct {
	const char *source_floor;
	const char *destination_floor;
	int sock;
	struct sockaddr_in server_addr;
} call_pad_t;

void call_pad_init(call_pad_t *, const char *, const char *);
void call_pad_deinit(call_pad_t *);
int call_pad_connect(call_pad_t *);
