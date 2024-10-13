#include <stdlib.h>

#include "global.h"
#include "posix.h"

int increment_floor(car_shared_mem *state, char *floor) {
	int floor_number;
	if (floor[0] == 'B') {
		floor_number = atoi(floor + 1);
		if (floor_number == 1) {
			set_string(state, floor, "1");
		} else {
			set_string(state, floor, "B%d", floor_number - 1);
		}
	} else {
		floor_number = atoi(floor);
		if (floor_number == 999) {
			return -1;
		} else {
			set_string(state, floor, "%d", floor_number + 1);
		}
	}


	return 0;
}

int decrement_floor(car_shared_mem *state, char *floor) {
	int floor_number;
	if (floor[0] == 'B') {
		floor_number = atoi(floor + 1);
		if (floor_number == 99) {
			return -1;
		} else {
			set_string(state, floor, "B%d", floor_number + 1);
		}
	} else {
		floor_number = atoi(floor);
		if (floor_number == 1) {
			set_string(state, floor, "B1");
		} else {
			set_string(state, floor, "%d", floor_number - 1);
		}
	}
	return 0;
}
