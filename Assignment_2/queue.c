#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

// Initalize the Queue
int initQueue(Queue *q, int size) {
    q->items = (struct customer_info **)malloc(size * sizeof(struct customer_info *));
    if (q->items == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for queue items\n");
        return -1;
    }
    q->front = 0;
    q->rear = -1;
    q->count = 0;
    q->size = size;
    return 0;
}

// Returns (1)True if Queue is empty otherwise (0)False
int isEmpty(Queue *q) {
    return q->count == 0;
}

// Returns (1)True if Queue is full otherwise (0)False
int isFull(Queue *q) {
    return q->count == q->size;
}

// To Enqueue a customer from the Queue
int enqueue(Queue *q, struct customer_info *value) {
    if (isFull(q)) {
        printf("The Queue has an overflow\n");
        return 0;
    } else {
        q->rear = (q->rear + 1) % q->size;
        q->items[q->rear] = value;
        q->count++;
        // Return 1 to for succesfull Enqueue opeartion
        return 1;
    }
}

// To Dequeue a customer from the Queue
struct customer_info *dequeue(Queue *q) {
    if (isEmpty(q)) {
        printf("Queue has an underflow\n");
        return NULL;
    } else {
        struct customer_info *item = q->items[q->front];
        q->front = (q->front + 1) % q->size;
        q->count--;
        return item;
    }
}