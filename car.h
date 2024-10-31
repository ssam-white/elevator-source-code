#pragma once

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>

#include "posix.h"

typedef struct
{
    int server_fd;
    struct sockaddr_in server_addr;
    const char *name;
    char *shm_name;
    const char *lowest_floor;
    const char *highest_floor;
    uint32_t delay;
    int fd;
    pthread_t door_thread;
    pthread_t level_thread;
    pthread_t receiver_thread;
    pthread_t connection_thread;
    bool connected_to_controller;
    car_shared_mem *state;
} car_t;

void car_init(car_t *, const char *, const char *, const char *, const char *);
void car_deinit(car_t *);

void *handle_doors(void *);
void *handle_level(void *);
void *handle_receiver(void *);
void *handle_connection(void *);

void open_doors(car_t *);
void close_doors(car_t *);

int usleep_cond(car_t *);
int timedwait_on_floor_and_status(car_t *);

int bounds_check_floor(car_t *car, char *);
int cdcmp_floors(car_shared_mem *);
void signal_controller(car_t *);
void sleep_delay(car_t *);
