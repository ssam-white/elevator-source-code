#pragma once

#include <unistd.h>
#include <stdbool.h>

typedef struct {
	const char *source_floor;
	const char *destination_floor;
	int sock;
	struct sockaddr_in server_addr;
} call_pad_t;

void call_pad_init(call_pad_t *, const char *, const char *);
void call_pad_deinit(call_pad_t *);
