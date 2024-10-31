#pragma once

#include <netinet/in.h>
#include <stdlib.h> // also provides size_t

#define PORT 3000
#define URL "127.0.0.1"
#define BACKLOG 10

bool connect_to_controller(int *, struct sockaddr_in *);

void send_looped(int, const void *, size_t);
void send_message(int, const char *, ...);
void recv_looped(int, void *, size_t);
char *receive_msg(int);
