#include "segel.h"
#include "request.h"
#include <stdbool.h>
#include "queue.h"

#define SUCCESS 0
// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too
void getargs(int *port, int *workers, int *queueSize, char **schedalg, int argc, char *argv[]) {
    if (argc < 5) { //todo: should be later changed to 5
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *workers = atoi(argv[2]);
    *queueSize = atoi(argv[3]);
    *schedalg = argv[4];
}

enum SchedalgPolicy {
    SCHPOL_BLOCK,
    SCHPOL_HEAD,
    SCHPOL_RANDOM,
    SCHPOL_TAIL
};

enum QueueType {
    WAITING_QUEUE,
    RUNNING_QUEUE
};

queue *waitingQueue;
queue *runningQueue;

int queueSize;          //size of the maximal requests (passed as an argument by the user)

void *workerRoutine(void* args) {
    int id = *((int*)args);

    while (1) {

        queueNode *request = popQueue(waitingQueue);
        request->id = pthread_self();

        pushQueue(runningQueue, *request);

        requestHandle(request->connection);
        close(request->connection);

        popNodeQueue(runningQueue, *request);
        break; //todo: delete this line
    }
    return NULL;
}

bool canBeInserted() {
    return waitingQueue->currentSize + runningQueue->currentSize < queueSize;
}

void blockPolicy(int connfd){ //todo: make sure the correct condition and mutex are listened
    while(!canBeInserted()){
        pthread_cond_wait(&waitingQueue->fullCond, &waitingQueue->mutex);
    }
    queueNode request;
    request.connection = connfd;
    struct timeval arrivalTime;
    gettimeofday(&arrivalTime, NULL);
    request.arrivalTime = arrivalTime;
    pushQueue(waitingQueue, request);
    pthread_cond_signal(&waitingQueue->emptyCond);
}

void addRequestToWaitingQueue(int connfd){
    queueNode request;

    struct timeval arrivalTime;
    gettimeofday(&arrivalTime, NULL);

    request.connection = connfd;
    request.arrivalTime = arrivalTime;

    pushQueue(waitingQueue, request);
}

void dropHeadPolicy(int connfd){
    while(waitingQueue->currentSize == 0 && !canBeInserted()){
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    if(removeAtIndexQueue(waitingQueue, 0) != SUCCESS){
        //todo: error handling
        exit(1);
    }

    addRequestToWaitingQueue(connfd);
}

void dropTailPolicy(int connfd){
    while(waitingQueue->currentSize == 0 && !canBeInserted()){
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    if(removeAtIndexQueue(waitingQueue, waitingQueue->currentSize - 1) != SUCCESS){
        //todo: error handling
        exit(1);
    }

    addRequestToWaitingQueue(connfd);
}

void dropRandomPolicy(int connfd){
    while(waitingQueue->currentSize == 0 && !canBeInserted()){
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    time_t t;
    srand((unsigned) time(&t));
    int n = waitingQueue->currentSize / 2;

    for(int i = 0; i < n; i++){
        removeAtIndexQueue(waitingQueue, rand() % waitingQueue->currentSize);
    }

    addRequestToWaitingQueue(connfd);
}


int main(int argc, char *argv[]) {
    int listenfd, connfd, clientlen;
    int port;
    int workers;
    char *schedalg;
    struct sockaddr_in clientaddr;

    getargs(&port, &workers, &queueSize, &schedalg, argc, argv);

    enum SchedalgPolicy policy;
    //todo: correct naming
    if (strcmp(schedalg, "block") == SUCCESS) {
        policy = SCHPOL_BLOCK;
    } else if (strcmp(schedalg, "dh") == SUCCESS) {
        policy = SCHPOL_HEAD;
    } else if (strcmp(schedalg, "random") == SUCCESS) {
        policy = SCHPOL_RANDOM;
    } else if (strcmp(schedalg, "dt") == SUCCESS) {
        policy = SCHPOL_TAIL;
    } else {
        //todo: error handling
        exit(1);
    }


    //initializing the queues
    waitingQueue = (queue *) malloc(sizeof(queue));
    runningQueue = (queue *) malloc(sizeof(queue));
    waitingQueue = create(queueSize);
    runningQueue = create(queueSize);
    //todo: allocation error handling

    //creating the pool of workers
    pthread_t *threadsPool = (pthread_t *) malloc(sizeof(pthread_t) * workers);
    for (int i = 0; i < workers; ++i) {
        int *id = malloc(sizeof(int));
        *id = i;
        if (pthread_create(&threadsPool[i], NULL, workerRoutine, id) != SUCCESS) {
            //todo: error handling
            exit(1);
        }
    }
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);

        queueNode request;
        request.connection = connfd;
        gettimeofday(&request.arrivalTime, NULL);


        if (canBeInserted()) {
            pushQueue(waitingQueue,request);
        } else {
            switch (policy) {
                case SCHPOL_BLOCK:
                    blockPolicy(connfd);
                    break;
                case SCHPOL_TAIL:
                    dropTailPolicy(connfd);
                    break;
                case SCHPOL_HEAD:
                    dropHeadPolicy(connfd);
                    break;
                default:
                    dropRandomPolicy(connfd);
                    break;
            }
        }

        //requestHandle(connfd);
        break; // todo: remove this line
        Close(connfd);
    }

}


    


 
