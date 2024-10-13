#include <stdlib.h>

#include "global.h"

int increment_floor(char *floor) {
	int floor_number;
	if (floor[0] == 'B') {
		floor_number = atoi(floor + 1);
		if (floor_number == 1) {
			sprintf(floor, "1");
		} else {
			sprintf(floor, "B%d", floor_number - 1);
		}
	} else {
		floor_number = atoi(floor);
		if (floor_number == 999) {
			return -1;
		} else {
			sprintf(floor, "%d", floor_number + 1);
		}
	}


	return 0;
}

int decrement_floor(char *floor) {
	int floor_number;
	if (floor[0] == 'B') {
		floor_number = atoi(floor + 1);
		if (floor_number == 99) {
			return -1;
		} else {
			sprintf(floor, "B%d", floor_number + 1);
		}
	} else {
		floor_number = atoi(floor);
		if (floor_number == 1) {
			sprintf(floor, "B1");
		} else {
			sprintf(floor, "%d", floor_number - 1);
		}
	}
	return 0;
}
