#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "call.h"
#include "globals.h"

int main (int argc, char *argv[]) {
	if (argc != 3 || !isValidFloor(argv[1]) || !isValidFloor(argv[2])) {
        printf("Invalid floor(s) specified.\n");
		return 1;
	}

	call_t call;
	call_init(&call, argv[1], argv[2]);

	if (call_connect_to_server(&call) < 0) {
		printf("Unable to connect to elevator system.\n");
		return 1;
	}


	printf("Not yet implemented\n");

	close(call.sock);

	return 0;
}


void call_init(call_t *call, const char *source_floor, const char *destination_floor) {
	call->source_floor = source_floor;
	call->destination_floor = destination_floor;
	call->sock = -1;
    memset(&(call->server_addr), 0, sizeof(call->server_addr));
}

int call_connect_to_server(call_t *call) {
	// create the socket
	if ((call->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return CALL_SOCK_CREATION_ERROR;
	}

	// set the sockets address
	call->server_addr.sin_family = AF_INET;
	call->server_addr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, URL, &call->server_addr.sin_addr) <= 0) {
		close(call->sock);
		return CALL_INVALID_ADDRESS_ERROR;
	}

	// connect to the server
	if (connect(call->sock, (struct sockaddr *) &call->server_addr, sizeof(call->server_addr)) < 0) {
		close(call->sock);
		return CALL_CONNECTION_ERROR;
	}

	return SUCCESS;
}

