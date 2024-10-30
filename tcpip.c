#include <stdlib.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "tcpip.h"

void send_looped(int fd, const void *buf, size_t sz) {
	const char *ptr = buf;
	size_t remain = sz;

	while (remain > 0) {
		ssize_t sent = write(fd, ptr, remain);
		if (sent == -1) {
			perror("write()");
			exit(1);
		}
		ptr += sent;
		remain -= (long unsigned) sent;
	}
}

void send_message(int fd, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char message[1024];
    int message_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (message_len < 0 || message_len >= sizeof(message)) {
        return;
    }

    uint32_t len = htonl((uint32_t) message_len);
    send_looped(fd, &len, sizeof(len));

    send_looped(fd, message, message_len);
}

void recv_looped(int fd, void *buf, size_t sz) {
	char *ptr = buf;
	size_t remain = sz;

	while (remain > 0) {
		ssize_t received = read(fd, ptr, remain);
		if (received == -1) {
			perror("read()");
			exit(1);
		}
		ptr += received;
		remain -= (unsigned long) received;
	}
}

char *receive_msg(int fd) {
	uint32_t nlen;
	recv_looped(fd, &nlen, sizeof(nlen));
	uint32_t len = ntohl(nlen);

	char *buf = malloc(len + 1);
	buf[len] = '\0';
	recv_looped(fd, buf, len);
	return buf;
}

