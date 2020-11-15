#include <stdio.h>
#include <stdlib.h>
#include "Queue.h"
#include <string.h>



struct Queue *queue_init(int capacity)
{
    struct Queue *queue = malloc(sizeof(struct Queue));

    queue->front = 0;
    queue->rear = capacity - 1;
    queue->size = 0;
    queue->capacity = capacity;
    queue->ptr = malloc(capacity * sizeof(int *));
    for (int i = 0; i < capacity; ++i)
        queue->ptr[i] = NULL;

    return queue;
}

int is_full(struct Queue *queue)
{
    return (queue->size == queue->capacity);
}

int is_empty(struct Queue *queue)
{
    return (queue->size == 0);
}

char *front(struct Queue *queue)
{
    if (is_empty(queue))
        return NULL;
    return queue->ptr[queue->front];
}

char *rear(struct Queue *queue)
{
    if (is_empty(queue))
        return NULL;
    return queue->ptr[queue->rear];
}

void enqueue(struct Queue *queue, char *item)
{
    if (is_full(queue))
    {
        printf("Queue is full!");
        return;
    }
    // Else
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->ptr[queue->rear] = item;
    queue->size = queue->size + 1;
}

char *dequeue(struct Queue *queue)
{
    if (is_empty(queue))
    {
        printf("Queue is empty!");
        return NULL;
    }
    // Else
    char *item = queue->ptr[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

void queue_destory(struct Queue *queue)
{
    for (int i = 0; i < queue->capacity; ++i)
        free(queue->ptr[i]);
    free(queue->ptr);
    free(queue);
}


int main()
{
    struct Queue *queue = queue_init(5);

    char *url1 = "http://ece252-1.uwaterloo.ca/~yqhuang/lab4/";
    char *url2 = "http://ece252-1.uwaterloo.ca/~yqhuang/lab3/index.html";
    char *url3 = "http://ece252-1.uwaterloo.ca/~yqhuang/lab4/Disguise.png";
    int length1 = sizeof("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/");
    int length2 = sizeof("http://ece252-1.uwaterloo.ca/~yqhuang/lab3/index.html");
    int length3 = sizeof("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/Disguise.png");

    char *URL1 = malloc(length1 * sizeof(char));
    char *URL2 = malloc(length2 * sizeof(char));
    char *URL3 = malloc(length3 * sizeof(char));
    strcpy(URL1, url1);
    strcpy(URL2, url2);
    strcpy(URL3, url3);


    enqueue(queue, URL1);
    char *ptr1 = dequeue(queue);
    enqueue(queue, URL2);
    char *ptr2 = dequeue(queue);
    enqueue(queue, URL3);
    char *ptr3 = dequeue(queue);

    printf("%s\n", ptr1);
    printf("%s\n", ptr2);
    printf("%s\n", ptr3);

    queue_destory(queue);

    return 0;
}