#include <pthread.h>
#include <stdint.h>

#include "globals.h"

typedef struct {
	const char *name;
	const char *lowest_floor;
	const char *highest_floor;
	const char *delay;
	int fd;
	car_shared_mem *state;
} car_t;

void car_init(car_t*, char*, char*, char*, char*);
bool create_shared_object( car_t*, const char*);
void print_car(car_t*);
