#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

/*
int main(void){
	
	Queue queue = createQueue();
	
	printf("front: %d\n", queue.front);

	queEnqueue(&queue, "temp str1");
	printf("enqueue: %s\n", queue.arr[0]);
	queEnqueue(&queue, "temp str2");
	printf("enqueue: %s\n", queue.arr[1]);
	queEnqueue(&queue, "temp str3");
	printf("enqueue: %s\n", queue.arr[2]);
	queEnqueue(&queue, "temp str4");
	printf("enqueue: %s\n", queue.arr[3]);
	queEnqueue(&queue, "temp str5");
	printf("enqueue: %s\n", queue.arr[4]);
	queEnqueue(&queue, "temp str6");
	printf("enqueue: %s\n", queue.arr[0]);
	queEnqueue(&queue, "temp str7");
	printf("enqueue: %s\n", queue.arr[1]);
	return 0;
}
*/

Queue* createQueue(void){
	Queue* q = (Queue*)malloc(sizeof(Queue));
	q->front = 0;
	return q;
}

void queEnqueue(Queue* q, char* str){

	q->arr[q->front] = str;
	
	q->front = (q->front + 1) % SIZE;

}

char* queFront(Queue* q){

	return q->arr[q->front-1];
}
