#include <stdlib.h>
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
	 char *r = dequeue(&q);
	free (r);
	char *t = dequeue(&q);
	free(t);

	enqueue(&q, "1", UP_FLOOR);
	char *a = dequeue(&q);
	free(a);

	queue_deinit(&q); // Uncommented

	return 0;
}
