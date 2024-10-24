#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

int floor_to_int(char *floor) {
	int floor_number;
	char temp_floor[5];
	strcpy(temp_floor, floor);
	if (temp_floor[0] == 'B') {
		temp_floor[0] = '-';
		floor_number = atoi(temp_floor);
	} else {
		floor_number = atoi(temp_floor) - 1;
	}
	return floor_number;
}

int floor_in_range(char *floor, char *lowest_floor, char *highest_floor) {

	int floor_number = floor_to_int(floor);
	int lowest_floor_number = floor_to_int(lowest_floor);
	int highest_floor_number = floor_to_int(highest_floor);

	if (floor_number < lowest_floor_number)
		return -1;
	else if (floor_number > highest_floor_number)
		return 1;
	else
		return 0;
}
