#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * This file implements the functionality of the call pad, allowing users to
 * initiate requests for elevator service by specifying source and destination
 * floors. It validates input, checks for duplicate requests on the same floor,
 * and manages the connection to the elevator controller. The implementation
 * focuses on efficient communication to ensure smooth operation of the elevator
 * requests.
 */

#include "call.h"
#include "global.h"
#include "tcpip.h"

int main(int argc, char *argv[])
{
    /* Validate the command line arguments. Expecting exactly two floor
     * arguments. */
    if (argc != 3 || !is_valid_floor(argv[1]) || !is_valid_floor(argv[2]))
    {
        printf("Invalid floor(s) specified.\n");
        return 1;
    }

    /* Initialize the call pad object with source and destination floors. */
    call_pad_t call_pad;
    call_pad_init(&call_pad, argv[1], argv[2]);

    /* Check if the source and destination floors are the same. */
    if (strcmp(call_pad.source_floor, call_pad.destination_floor) == 0)
    {
        printf("You are already on that floor!\n");
        /* Deinitialise the call pad to avoid memory leaks. */
        call_pad_deinit(&call_pad);
        return 0;
    }

    /* Attempt to connect the call pad to the controller. */
    if (!connect_to_controller(&call_pad.sock, &call_pad.server_addr))
    {
        printf("Unable to connect to elevator system.\n");
        return 1;
    }

    /* Handle the call request and deinitialise the call pad afterwards. */
    handle_call(&call_pad);
    call_pad_deinit(&call_pad);

    return 0;
}

/*
 * Initialize the call pad object.
 * Set the source and destination floors from command line arguments,
 * initialise the socket descriptor to an invalid state, and clear the server
 * address structure.
 */
void call_pad_init(call_pad_t *call_pad, const char *source_floor,
                   const char *destination_floor)
{
    call_pad->source_floor = source_floor;
    call_pad->destination_floor = destination_floor;
    call_pad->sock = -1; // Set socket descriptor to an invalid state
    memset(&(call_pad->server_addr), 0,
           sizeof(call_pad->server_addr)); // Clear the server address
}

/*
 * Deinitialise the call pad and free up resources.
 * Set floor pointers to NULL and close the socket if it is valid.
 */
void call_pad_deinit(call_pad_t *call_pad)
{
    call_pad->source_floor = NULL;      // Clear source floor reference
    call_pad->destination_floor = NULL; // Clear destination floor reference
    if (call_pad->sock != -1) // Check if the socket is valid before closing
    {
        close(call_pad->sock);
        call_pad->sock = -1; // Mark socket as invalid
    }
}

/*
 * Send the call request to the controller and handle the response.
 * Sends a message with the source and destination floors, then waits for a
 * response. Parses the response to check the status of the call request.
 */
void handle_call(call_pad_t *call_pad)
{
    /* Send the call request and wait for a response from the controller. */
    send_message(call_pad->sock, "CALL %s %s", call_pad->source_floor,
                 call_pad->destination_floor);
    char *response =
        receive_msg(call_pad->sock); // Receive the response message

    /* Tokenize the response to extract its values. */
    char *saveptr;
    const char *response_type = strtok_r(response, " ", &saveptr);

    /* Check if the controller found a car to service the call request. */
    if (strcmp(response_type, "UNAVAILABLE") == 0)
    {
        printf("Sorry, no car is available to take this request.\n");
    }
    else
    {
        const char *car_name = strtok_r(NULL, " ", &saveptr);
        printf("Car %s is arriving.\n", car_name); // Announce the arriving car
    }

    free(response); // Free the allocated response memory
}
