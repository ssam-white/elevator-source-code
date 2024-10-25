#include <stdio.h>

#include "queue.h"

int main(void)
{
	queue_t q;
	queue_init(&q);

	char *sf = "1";
	char *df = "2";

	enqueue(&q, sf, UP_FLOOR);
	enqueue(&q, df, UP_FLOOR);
	printf("%s\n", dequeue(&q));
	printf("%s\n", dequeue(&q));


	enqueue(&q, df, DOWN_FLOOR);
	enqueue(&q, sf, DOWN_FLOOR);
	printf("%s\n", dequeue(&q));
	printf("%s\n", dequeue(&q));



	return 0;
}
