#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "call.h"

int main (int argc, char *argv[]) {
	call_t call;

	if (argc != 3) {
        fprintf(stderr, "Error: Incorrect number of command line arguments. Expected 2, got %d.\n", argc - 1);
		return 1;
	}

	call.source_floor = argv[1];
	call.destination_floor = argv[2];

	if (!call_connect_to_server(&call)) {
		perror("Unable to connect to elevator system.");
		return 1;
	}


	printf("Not yet implemented\n");

	close(call.sock);

	return 0;
}


bool call_connect_to_server(call_t *call) {
	// create the socket
	if ((call->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Socket creation failed.\n");
		return false;
	}

	// set the sockets address
	call->server_addr.sin_family = AF_INET;
	call->server_addr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, "127.0.0.1", &call->server_addr.sin_addr) <= 0) {
		perror("Invalid address\n");
		close(call->sock);
		return false;
	}

	// connect to the server
	if (connect(call->sock, (struct sockaddr *) &call->server_addr, sizeof(call->server_addr)) < 0) {
		close(call->sock);
		return false;
	}

	return true;
}

