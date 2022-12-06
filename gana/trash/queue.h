#pragma once

#define SIZE 5

typedef struct queue{
        char* arr[SIZE];
        int front;
} Queue;

Queue* createQueue(void);
void queEnqueue(Queue* q, char* str);
char* queFront(Queue* q);

