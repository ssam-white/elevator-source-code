#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tcpip.h"

/*
 *  Creates a tcp/ip server on localhost port 3000
 */
void server_init(int *fd, struct sockaddr_in *sock)
{
    memset(sock, 0, sizeof(*sock));
    sock->sin_family = AF_INET;
    sock->sin_port = htons(3000);
    sock->sin_addr.s_addr = htonl(INADDR_ANY);

    // Create the socket
    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd == -1)
    {
        perror("socket()");
        exit(1);
    }

    // Set socket options to allow reuse of address
    int opt_enable = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt_enable,
                   sizeof(opt_enable)) == -1)
    {
        perror("setsockopt()");
        close(*fd); // Close the socket before exiting
        exit(1);
    }

    // Bind the socket to the address and port
    if (bind(*fd, (const struct sockaddr *)sock, sizeof(*sock)) == -1)
    {
        perror("bind()");
        close(*fd); // Close the socket before exiting
        exit(1);
    }

    // Start listening for incoming connections
    if (listen(*fd, 10) == -1)
    {
        perror("listen()");
        close(*fd); // Close the socket before exiting
        exit(-1);
    }
}

bool connect_to_controller(int *sd, struct sockaddr_in *sockaddr)
{
    // create the socket
    *sd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sd < 0)
    {
        return false;
    }

    // set the sockets address
    sockaddr->sin_family = AF_INET;
    sockaddr->sin_port = htons(PORT);
    if (inet_pton(AF_INET, URL, &sockaddr->sin_addr) <= 0)
    {
        close(*sd);
        return false;
    }

    // connect to the server
    if (connect(*sd, (struct sockaddr *)sockaddr, sizeof(*sockaddr)) < 0)
    {
        close(*sd);
        *sd = -1;
        return false;
    }

    return true;
}

void send_looped(int fd, const void *buf, size_t sz)
{
    const char *ptr = buf;
    size_t remain = sz;

    while (remain > 0)
    {
        ssize_t sent = write(fd, ptr, remain);
        if (sent == -1)
        {
            perror("write()");
            exit(1);
        }
        ptr += sent;
        remain -= (long unsigned)sent;
    }
}

void send_message(int fd, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char message[1024];
    int message_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (message_len < 0 || message_len >= (int)sizeof(message))
    {
        return;
    }

    uint32_t len = htonl((uint32_t)message_len);
    send_looped(fd, &len, sizeof(len));

    send_looped(fd, message, (size_t)message_len);
}

void recv_looped(int fd, void *buf, size_t sz)
{
    char *ptr = buf;
    size_t remain = sz;

    while (remain > 0)
    {
        ssize_t received = read(fd, ptr, remain);
        if (received == -1)
        {
            perror("read()");
            exit(1);
        }
        ptr += received;
        remain -= (unsigned long)received;
    }
}

char *receive_msg(int fd)
{
    uint32_t nlen;
    recv_looped(fd, &nlen, sizeof(nlen));
    uint32_t len = ntohl(nlen);

    char *buf = malloc(len + 1);
    buf[len] = '\0';
    recv_looped(fd, buf, len);
    return buf;
}
