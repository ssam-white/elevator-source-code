#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

int main(void)
{

    queue_t q;
    queue_init(&q);

    enqueue_pair(&q, "3", "6");
    dequeue(&q);
    enqueue_pair(&q, "7", "4");
    dequeue(&q);
    enqueue_pair(&q, "8", "4");
    enqueue_pair(&q, "6", "5");

    print_queue(&q);

    queue_deinit(&q); // Uncommented

    return 0;
}
