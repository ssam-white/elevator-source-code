#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

typedef enum {
	SUCCESS = 0,
	CALL_CONNECTION_ERROR = -1,
	CALL_SOCK_CREATION_ERROR = -2,
	CALL_INVALID_ADDRESS_ERROR = -3,
} call_error_t;

typedef struct {
	const char *source_floor;
	const char *destination_floor;
	int sock;
	struct sockaddr_in server_addr;
} call_t;

void call_init(call_t *, const char *, const char *);
int call_connect_to_server(call_t *);
