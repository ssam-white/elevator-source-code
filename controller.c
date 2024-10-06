
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 3000
#define BACKLOG 10

int main()
{

	struct sockaddr_in server_addr, client_addr;

	// Ignore SIGPIPE to prevent crashing when the client disconnects abruptly
	signal(SIGPIPE, SIG_IGN);

	// create the socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("failed to create socket");
		exit(EXIT_FAILURE);
	}


	// Step 3: Bind the socket to IP address and port
	server_addr.sin_family = AF_INET;          // Use IPv4
	server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any available network interface
	server_addr.sin_port = htons(PORT);        // Bind to port 3000 (network byte order)

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Bind failed");
		close(server_fd);
		exit(EXIT_FAILURE);
	}


	// Step 4: Listen for incoming connections
	if (listen(server_fd, BACKLOG) < 0) {
		perror("Listen failed");
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	printf("Server listening on port %d...\n", PORT);

	while (1) {

	}

	return 0;
}
