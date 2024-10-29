#include <stdbool.h>

#include "posix.h"

typedef struct {
	char *car_name;
	char *shm_name;
	int fd;
	uint8_t emergency_msg_sent;
	uint8_t overload_msg_sent;
	car_shared_mem *state;
} safety_t;

void safety_init(safety_t *, char *);
bool safety_connect(safety_t *);
