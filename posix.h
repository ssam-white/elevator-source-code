#include "global.h"
#include <stdbool.h>
#include <stdint.h>

void set_flag(car_shared_mem *, uint8_t *, uint8_t);
void set_status(car_shared_mem *, char *);
void set_open_button(car_shared_mem *, uint8_t);
bool open_button_is(car_shared_mem *, uint8_t);
