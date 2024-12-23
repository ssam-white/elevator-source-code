#include <stdbool.h>

#include "posix.h"

typedef struct
{
    char *car_name;
    char *shm_name;
    int fd;
    uint8_t emergency_msg_sent;
    uint8_t overload_msg_sent;
    car_shared_mem *state;
} safety_t;

void safety_init(safety_t *, char *);
void safety_deinit(safety_t *);

bool is_shm_int_fields_valid(const car_shared_mem *);
bool is_shm_status_valid(const car_shared_mem *);
bool is_shm_obstruction_valid(const car_shared_mem *);
bool is_shm_data_valid(const car_shared_mem *);
