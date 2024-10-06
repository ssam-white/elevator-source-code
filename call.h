#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 3000

typedef struct {
	const char *source_floor;
	const char *destination_floor;
	int sock;
	struct sockaddr_in server_addr;
} call_t;

bool call_connect_to_server(call_t *);
