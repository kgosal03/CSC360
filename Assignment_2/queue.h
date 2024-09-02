#ifndef QUEUE_H_
#define QUEUE_H_

// Define the Queue structure
typedef struct {
    struct customer_info **items;
    int front;
    int rear;
    int count;
    int size;
} Queue;

// Function prototypes

/**
 * @brief Initializes the queue with a given size.
 * 
 * @param q Pointer to the Queue structure to initialize.
 * @param size Maximum number of elements that the queue can hold.
 * @return 0 if successful, otherwise -1.
 */
int initQueue(Queue *q, int size);

/**
 * @brief Adds an element to the rear of the queue.
 * 
 * @param q Pointer to the Queue structure.
 * @param value Pointer to the customer_info struct to enqueue.
 * @return 1 if successful, 0 if the queue is full.
 */
int enqueue(Queue *q, struct customer_info *value);

/**
 * @brief Removes and returns the element from the front of the queue.
 * 
 * @param q Pointer to the Queue structure.
 * @return Pointer to the customer_info struct that was dequeued, NULL if the queue is empty.
 */
struct customer_info *dequeue(Queue *q);

/**
 * @brief Checks if the queue is empty.
 * 
 * @param q Pointer to the Queue structure.
 * @return 1 if the queue is empty, 0 otherwise.
 */
int isEmpty(Queue *q);

/**
 * @brief Checks if the queue is full.
 * 
 * @param q Pointer to the Queue structure.
 * @return 1 if the queue is full, 0 otherwise.
 */
int isFull(Queue *q);

#endif /* QUEUE_H_ */
