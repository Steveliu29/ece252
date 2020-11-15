#ifndef UNTITLED_QUEUE_H
#define UNTITLED_QUEUE_H


#include <stdio.h>
#include <stdlib.h>



// Reference from Geeks for geeks
// https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation


// Queue struct
struct Queue {
    int front;
    int rear;
    int size;
    int capacity;
    char **ptr;
};


// Member functions
struct Queue *queue_init(int capacity);
int is_full(struct Queue *queue);
int is_empty(struct Queue *queue);
char *front(struct Queue *queue);
char *rear(struct Queue *queue);
void enqueue(struct Queue *queue, char *item);
char *dequeue(struct Queue *queue);
void queue_destory(struct Queue *queue);


#endif //UNTITLED_QUEUE_H