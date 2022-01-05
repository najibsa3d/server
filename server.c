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
typedef struct stat_t {
    int threadId;
    int requestCount;
    int staticCount;
    int dynamicCount;
} Stats;

Stats *create_stat(int threadId) {
    Stats *s = malloc(sizeof(s));
    s->staticCount = 0;
    s->threadId = threadId;
    s->dynamicCount = 0;
    s->requestCount = 0;
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
    //int id = *((int*)args);

    while (1) {

        queueNode *request = popQueue(waitingQueue);
        request->threadID = pthread_self();
        pthread_mutex_lock(&m);
        int i = 0;
        for (i = 0; i < workers; i++) {
            if (request->threadID == threadsPool[i]->threadId) {
                threadsPool[i]->requestCount++;
                break;
            }
        }


        struct timeval dispatchTime;
        gettimeofday(&dispatchTime, NULL);
        request->dispatchTime = dispatchTime;
        pushQueue(runningQueue, *request);
        int res = requestHandle(request->connection);
        if (res == 0)
            threadsPool[i]->dynamicCount++;
        else if (res == 1)
            threadsPool[i]->staticCount++;

        close(request->connection);

        pthread_mutex_unlock(&m);

        popNodeQueue(runningQueue, *request);
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
    pthread_cond_signal(&waitingQueue->emptyCond);
}


void dropHeadPolicy(int connfd) {
    while (waitingQueue->currentSize == 0 && !canBeInserted()) {
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    if (removeAtIndexQueue(waitingQueue, 0) != SUCCESS) {
        //todo: error handling
        exit(1);
    }

    addRequestToWaitingQueue(connfd);
}

void dropTailPolicy(int connfd) {
    while (waitingQueue->currentSize == 0 && !canBeInserted()) {
        pthread_cond_wait(&waitingQueue->emptyCond, &waitingQueue->mutex);
    }

    if (removeAtIndexQueue(waitingQueue, waitingQueue->currentSize - 1) != SUCCESS) {
        //todo: error handling
        exit(1);
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
    printf("1");
    int listenfd, connfd, clientlen;
    int port;

    char *schedalg;
    struct sockaddr_in clientaddr;

    pthread_mutex_init(&m, NULL);

    getargs(&port, &workers, &queueSize, &schedalg, argc, argv);
    printf("2");

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

    printf("3");

    //initializing the queues
    waitingQueue = (queue *) malloc(sizeof(queue));
    runningQueue = (queue *) malloc(sizeof(queue));
    waitingQueue = create(queueSize);
    runningQueue = create(queueSize);
    //todo: allocation error handling

    //creating the pool of workers
    threadsPool = (Stats **) malloc(sizeof(Stats *) * workers);
    printf("4");

    for (int i = 0; i < workers; ++i) {
        pthread_t t;
        threadsPool[i] = create_stat(i);
        int *id = malloc(sizeof(int));
        *id = i;

        if (pthread_create(&t, NULL, workerRoutine, id) != SUCCESS) {
            //todo: error handling
            exit(1);
        }
        threadsPool[i] = create_stat(t);
    }
    printf("5");

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t * ) & clientlen);

        queueNode request;
        request.connection = connfd;
        gettimeofday(&request.arrivalTime, NULL);


        if (canBeInserted()) {
            pushQueue(waitingQueue, request);
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
        Close(connfd);
        printf("6");

    }

}


    


 
