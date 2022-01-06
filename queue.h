//
// Created by student on 1/3/22.
//

#ifndef OS_HW3_QUEUE_H
#define OS_HW3_QUEUE_H
#include <pthread.h>
#include <sys/time.h>


typedef struct queueNode_t {
    pthread_t threadID;
    struct timeval arrivalTime;
    struct timeval dispatchTime;
    int connection;
} queueNode;


typedef struct Queue_t{
    int maxSize;
    int currentSize;
    queueNode **nodes;
    pthread_mutex_t mutex;
    pthread_cond_t fullCond;
    pthread_cond_t emptyCond;
} queue;

queue *create(int size);
int pushQueue(queue* q, queueNode node);
queueNode* popQueue(queue* q);
void popNodeQueue(queue* q, queueNode node);
void queueDestroy(queue* q);
int removeAtIndexQueue(queue* q, int index);

#endif //OS_HW3_QUEUE_H
