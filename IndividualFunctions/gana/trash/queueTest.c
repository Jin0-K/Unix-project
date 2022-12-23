#include <stdio.h>
#include "queue.h"

int main(void){

	printf("size: %d\n", SIZE);

	
	Queue* q = createQueue();
	queEnqueue(q, "test str bb");
	printf("front: %d\n", q->front);
	printf("q: %s\n", queFront(q));
	

	return 0;
}
