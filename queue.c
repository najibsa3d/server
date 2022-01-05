#include "queue.h"
#include <stdlib.h>

queue *create(int size){
    queue *this = malloc(sizeof(queue));
    if(!this){
        return NULL;
    }
    this->maxSize = size;
    this->currentSize = 0;

    queueNode *nodes = malloc(sizeof(queueNode) * size);
    if(!nodes){
        free(this);
        return NULL;
    }
    *this->nodes = nodes;

    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->fullCond, NULL);
    pthread_cond_init(&this->emptyCond, NULL);
    return this;
}

int pushQueue(queue* q, queueNode node){
    if(!q)
        return -1;

    pthread_mutex_lock(&q->mutex);
    while(q->currentSize >= q->maxSize){
        pthread_cond_wait(&q->fullCond, &q->mutex);
    }
    *q->nodes[q->currentSize] = node;
    q->currentSize++;
    pthread_cond_signal(&q->emptyCond);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void popNodeQueue(queue* q, queueNode node){
    if(!q)
        return;

    for(int i = 0; i < q->currentSize; i++){
        if(q->nodes[i]->connection == node.connection){
            removeAtIndexQueue(q, i);
            return;
        }
    }
}

queueNode* popQueue(queue* q){
    if(!q)
        return NULL;

    pthread_mutex_lock(&q->mutex);

    while(q->currentSize == 0){
        pthread_cond_wait(&q->emptyCond, &q->mutex);
    }
    queueNode* node = malloc(sizeof(*node));
    node = q->nodes[q->currentSize - 1];
    q->currentSize--;

    pthread_cond_signal(&q->fullCond);

    pthread_mutex_unlock(&q->mutex);
    return node;
}

int removeAtIndexQueue(queue* q, int index){
    if(!q)
        return -1;
    if(index >= q->currentSize)
        return -2;
    if(index == q->currentSize - 1){
        if(popQueue(q))
            return 0;
        return -1;
    }
    pthread_mutex_lock(&q->mutex);
    q->nodes[index] = NULL;
    int temp = 1;
    for(int i = 0; i < q->currentSize; i++){
        if(q->nodes[i]){
            i++;
            temp++;
        }
        else if(temp < q->currentSize){
            while(!q->nodes[temp]){
                temp++;
            }
            q->nodes[i] = q->nodes[temp];
            q->nodes[temp] = NULL;
            i = temp - 1;
        }
    }
    q->currentSize--;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void queueDestroy(queue* q){
    if(!q)
        return;

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->fullCond);
    pthread_cond_destroy(&q->emptyCond);

    free(q->nodes);
    free(q);
}