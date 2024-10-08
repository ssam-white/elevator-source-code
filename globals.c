#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "globals.h"

bool is_valid_floor(char *floor) {
	size_t len = strlen(floor);

	if (len > 3) {
		return false;
	}

	if (floor[0] == 'B') {
		if (len == 1) {
			return false;
		}

		for (size_t i = 1; i < len; i++) {
			if (!isdigit((unsigned char) floor[i])) {
				return false;
			}
		}
		return true;

	}

	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char) floor[i])) {
			return false;
		}
	}

	return true;
}


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
		remain -= sent;
	}
}

void send_message(int fd, const char *buf) {
	uint32_t len = htonl(strlen(buf));
	send_looped(fd, &len, sizeof(len));
	send_looped(fd, buf, strlen(buf));
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
		remain -= received;
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

void set_field(car_shared_mem *state, void *field, void *new_value, size_t size) {
	pthread_mutex_lock(&state->mutex);
	memcpy(field, new_value, size);
	pthread_cond_signal(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}
