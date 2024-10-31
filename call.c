#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "call.h"
#include "global.h"
#include "tcpip.h"

int main(int argc, char *argv[])
{
    // validate the command line arguments
    if (argc != 3 || !is_valid_floor(argv[1]) || !is_valid_floor(argv[2]))
    {
        printf("Invalid floor(s) specified.\n");
        return 1;
    }

    // initialize the call pad object
    call_pad_t call_pad;
    call_pad_init(&call_pad, argv[1], argv[2]);

    if (strcmp(call_pad.source_floor, call_pad.destination_floor) == 0)
    {
        printf("You are already on that floor!\n");
		call_pad_deinit(&call_pad);
        return 0;
   }

    // connect the call pad to the controller
    if (!connect_to_controller(&call_pad.sock, &call_pad.server_addr))
    {
        printf("Unable to connect to elevator system.\n");
        return 1;
    }

	handle_call(&call_pad);
    call_pad_deinit(&call_pad);

    return 0;
}

void call_pad_init(call_pad_t *call_pad, const char *source_floor,
                   const char *destination_floor)
{
    call_pad->source_floor = source_floor;
    call_pad->destination_floor = destination_floor;
    call_pad->sock = -1;
    memset(&(call_pad->server_addr), 0, sizeof(call_pad->server_addr));
}

void call_pad_deinit(call_pad_t *call_pad)
{
    call_pad->source_floor = NULL;
    call_pad->destination_floor = NULL;
    if (call_pad->sock == 0)
    {
        close(call_pad->sock);
        call_pad->sock = -1;
    }
}

void handle_call(call_pad_t *call_pad)
{
    send_message(call_pad->sock, "CALL %s %s", call_pad->source_floor, call_pad->destination_floor);
    char *response = receive_msg(call_pad->sock);


	char *saveptr;
	const char *response_type = strtok_r(response, " ", &saveptr);

    if (strcmp(response_type, "UNAVAILABLE") == 0)
    {
        printf("Sorry, no car is available to take this request.\n");
    }
    else
    {
		const char *car_name = strtok_r(NULL, " ", &saveptr);
        printf("Car %s is arriving.\n", car_name);
    }

    free(response);
}
