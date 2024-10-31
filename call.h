#pragma once

#include <stdbool.h>
#include <unistd.h>

typedef struct
{
    const char *source_floor;
    const char *destination_floor;
    int sock;
    struct sockaddr_in server_addr;
} call_pad_t;

void call_pad_init(call_pad_t *, const char *, const char *);
void call_pad_deinit(call_pad_t *);
void handle_call(call_pad_t *);
