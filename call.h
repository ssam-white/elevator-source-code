#pragma once

#include <netinet/in.h> // Include for struct sockaddr_in
#include <stdbool.h>
#include <unistd.h>

/*
 * This header file defines the `call_pad_t` structure, which contains fields
 * for managing requests to the elevator controller, and declares the functions
 * for initializing and handling call pad operations. For more detailed
 * information on the implementation and functionality, please refer to
 * `call.c`.
 */

/*
 * Structure representing a call pad in the elevator system
 */
typedef struct
{
    const char *source_floor;      // The floor from which the call is made
    const char *destination_floor; // The desired destination floor
    int sock; // Socket for communication with the controller
    struct sockaddr_in
        server_addr; // Server address for the elevator controller
} call_pad_t;

// Initializes the call pad with specified source and destination floors
void call_pad_init(call_pad_t *, const char *, const char *);
// Deinitializes the call pad and frees any allocated resources
void call_pad_deinit(call_pad_t *);
// Handles the call request from the call pad
void handle_call(const call_pad_t *);
