#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define MILLISECOND 1000 // 1ms

typedef struct {
	pthread_mutex_t mutex;           // Locked while accessing struct contents
	pthread_cond_t cond;             // Signalled when the contents change
	char current_floor[4];           // C string in the range B99-B1 and 1-999
	char destination_floor[4];       // Same format as above
	char status[8];                  // C string indicating the elevator's status
	uint8_t open_button;             // 1 if open doors button is pressed, else 0
	uint8_t close_button;            // 1 if close doors button is pressed, else 0
	uint8_t door_obstruction;        // 1 if obstruction detected, else 0
	uint8_t overload;                // 1 if overload detected
	uint8_t emergency_stop;          // 1 if stop button has been pressed, else 0
	uint8_t individual_service_mode; // 1 if in individual service mode, else 0
	uint8_t emergency_mode;          // 1 if in emergency mode, else 0
} car_shared_mem;

void init_shm(car_shared_mem *);
void reset_shm(car_shared_mem *);

bool is_valid_floor(char *);
void set_field(car_shared_mem *, void *, void *, size_t);
char *get_shm_name(const char *);
