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
        exit(0);
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


Stats *create_stat(pthread_t threadId) {
    Stats *s = malloc(sizeof(Stats));
    if(!s){
        return NULL;
    }
    s->staticCount = malloc(sizeof(int));
    *s->staticCount = 0;
    s->threadId = threadId;
    s->dynamicCount = malloc(sizeof(int));
    *s->dynamicCount = 0;
    s->requestCount = 0;
    s->index = -1;
    return s;
}

pthread_mutex_t m;
Stats **threadsPool;
int *threadCount;
queue *waitingQueue;
queue *runningQueue;
int workers;

int queueSize;          //size of the maximal requests (passed as an argument by the user)

void *workerRoutine(void *args) {


    while (1) {
        //fprintf(stderr,"111111111111111111\n");

        pthread_mutex_lock(&m);
        queueNode *request = popQueueTail(waitingQueue);
        request->threadID = pthread_self();
        int i = 0;
        for (i = 0; i < workers; i++) {
            if (request->threadID == threadsPool[i]->threadId) {
                fprintf(stderr,"2222222222222222222\n");

                threadsPool[i]->requestCount++;
                threadsPool[i]->index = i;
                break;
            }
            //fprintf(stderr,"33333333333333\n");

        }
        Stats* temp = threadsPool[i];
        //fprintf(stderr,"2222222222222222222\n");
        struct timeval dispatchTime;
        gettimeofday(&dispatchTime, NULL);
        request->dispatchTime = dispatchTime;
        threadsPool[i]->dispatchTime = dispatchTime;
        threadsPool[i]->arrivalTime = request->arrivalTime;


        fprintf(stderr,"dddddddddddd\n");
        pushQueue(runningQueue, *request);
        fprintf(stderr,"XXXXXrunningqueue size:%d\n",runningQueue->currentSize);
        pthread_mutex_unlock(&m);


        /*Stats* stat = malloc(sizeof(Stats));

        if(!stat)
            return;
        //fprintf(stderr,"zzzzzzzzzzzz\n");

        stat->threadId = i;
        //fprintf(stderr,"yyyyyyyyyyyyyyy\n");

        stat->dispatchTime = dispatchTime;
        //fprintf(stderr,"XXXXXXXXXXXXXXXXXxx\n");
        stat->arrivalTime = request->arrivalTime;

        stat->requestCount = threadsPool[i]->requestCount;
        *stat->staticCount = *threadsPool[i]->staticCount;
        *stat->dynamicCount = *threadsPool[i]->dynamicCount;*/


        //fprintf(stderr,"**************\n");
        requestHandle(request->connection,temp);
        //fprintf(stderr,"333333333333333333\n");

        close(request->connection);
        //fprintf(stderr,"4444444444444444\n");

        popNodeQueue(runningQueue, *request);
        //fprintf(stderr,"99999999999999\n");

    }
    return NULL;
}

bool canBeInserted() {
    return waitingQueue->currentSize + runningQueue->currentSize < queueSize;
}

void addRequestToWaitingQueue(int connfd) {
    queueNode request;

    struct timeval arrivalTime;
    gettimeofday(&arrivalTime, NULL);

    request.connection = connfd;
    request.arrivalTime = arrivalTime;

    pushQueue(waitingQueue, request);
}

void blockPolicy(int connfd) { //todo: make sure the correct condition and mutex are listened
    while (!canBeInserted()) {
        pthread_cond_wait(&waitingQueue->fullCond, &waitingQueue->mutex);
    }
    addRequestToWaitingQueue(connfd);
    pthread_cond_broadcast(&waitingQueue->emptyCond);
}


void dropHeadPolicy(int connfd) {
    while (waitingQueue->currentSize == 0 && !canBeInserted()) {
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    if (removeAtIndexQueue(waitingQueue, 0) != SUCCESS) {
        //todo: error handling
        exit(0);
    }

    addRequestToWaitingQueue(connfd);
}

void dropTailPolicy(int connfd) {
    while (waitingQueue->currentSize == 0 && !canBeInserted()) {
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    if (removeAtIndexQueue(waitingQueue, waitingQueue->currentSize - 1) != SUCCESS) {
        //todo: error handling
        exit(0);
    }

    addRequestToWaitingQueue(connfd);
}

void dropRandomPolicy(int connfd) {
    while (waitingQueue->currentSize == 0 && !canBeInserted()) {
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    time_t t;
    srand((unsigned) time(&t));
    int n = ceil(waitingQueue->currentSize / 2);

    for (int i = 0; i < n; i++) {
        removeAtIndexQueue(waitingQueue, rand() % waitingQueue->currentSize);
    }

    addRequestToWaitingQueue(connfd);
}


int main(int argc, char *argv[]) {
    //fprintf(stderr,"1\n");
    int listenfd, connfd, clientlen;
    int port;

    char *schedalg;
    struct sockaddr_in clientaddr;

    pthread_mutex_init(&m, NULL);

    getargs(&port, &workers, &queueSize, &schedalg, argc, argv);
    //fprintf(stderr,"2\n");
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
        exit(0);
    }

    //fprintf(stderr,"3\n");
    //initializing the queues
    waitingQueue = (queue *) malloc(sizeof(queue));
    if(!waitingQueue)
        return 0;
    runningQueue = (queue *) malloc(sizeof(queue));
    if(!runningQueue){
        free(waitingQueue);
        return 0;
    }
    waitingQueue = create(queueSize);
    runningQueue = create(queueSize);
    //todo: allocation error handling
    //fprintf(stderr,"1");
    //creating the pool of workers
    threadsPool = (Stats **) malloc(sizeof(Stats *) * workers);
    if(!threadsPool){
        free(waitingQueue);
        free(runningQueue);
        return 0;
    }
    //fprintf(stderr,"1");
    for (int i = 0; i < workers; ++i) {
        pthread_t t;
        //pthread_create(&t, NULL, workerRoutine, NULL);

        if (pthread_create(&t, NULL, workerRoutine, NULL) != SUCCESS) {
            fprintf(stderr,"hahahahahahahaha\n");

            //todo: error handling
            exit(0);
        }
        fprintf(stderr,"hehehehehehehehe\n");

        threadsPool[i] = create_stat(t);
        if(!threadsPool[i]){
            for(int j = 0; j <i; j++){
                free(threadsPool[j]);
            }
            free(threadsPool);
            exit(0);
        }
    }
    //fprintf(stderr,"1");
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        fprintf(stderr,"5\n");
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t * ) & clientlen);
        fprintf(stderr,"6\n");
        queueNode request;
        request.connection = connfd;
        gettimeofday(&request.arrivalTime, NULL);
        fprintf(stderr,"7\n");
        fprintf(stderr,"queuesize: %d\n",queueSize);
        fprintf(stderr,"workers: %d\n",workers);
        fprintf(stderr,"runningqueue size: %d\n",runningQueue->currentSize);
        fprintf(stderr,"waitingqueue size: %d\n",waitingQueue->currentSize);

        if (canBeInserted()) {
            //fprintf(stderr,"Waiting %d\n",waitingQueue->currentSize);
            //fprintf(stderr,"running %d\n",runningQueue->currentSize);
            //fprintf(stderr,"size %d\n",queueSize);

            pushQueue(waitingQueue, request);
            fprintf(stderr,"LLLLLLLLL\n");

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
        fprintf(stderr,"8\n");
    }
    fprintf(stderr,"9\n");

    Close(connfd);
    queueDestroy(waitingQueue);
    queueDestroy(runningQueue);
    pthread_mutex_destroy(&m);


}



    


 
