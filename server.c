#include "segel.h"
#include "request.h"
#include <stdbool.h>
#include "list.h"
#include "assert.h"
#include <unistd.h>
#include "stdbool.h"
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
int listSize;
int workers;
pthread_mutex_t mutex;
LinkedList* runningList;
LinkedList* waitingList;
Stats** threads;


bool debug = false;

/*
 * TODO:
 *  1.add mutex in the right place
 *  2.add cond vars
 *  3.tests
 */


void getargs(int *port, int *w, int *size, char **schedalg, int argc, char *argv[]) {

    if (argc < 5) { //todo: should be later changed to 5
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    *port = atoi(argv[1]);
    *w = atoi(argv[2]);
    *size = atoi(argv[3]);
    *schedalg = argv[4];
}

enum SchedalgPolicy {
    SCHPOL_BLOCK,
    SCHPOL_HEAD,
    SCHPOL_RANDOM,
    SCHPOL_TAIL
};

Stats *createStat(pthread_t threadId) {
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

void *workerRoutine(void *args) {

    while(1) {
        if(debug) fprintf(stderr, "SEXSEX11\n");
        fprintf(stderr, "%ld lock 1\n", pthread_self());

        pthread_mutex_lock(&waitingList->listMutex);
        if(debug) fprintf(stderr, "SEXSEX1166, %ld\n", pthread_self());
        while (waitingList->size == 0) {
            fprintf(stderr, "thread %ld is waiting\n", pthread_self());
            pthread_cond_wait(&waitingList->emptyCond, &waitingList->listMutex);

        }
        fprintf(stderr, "%ld here1\n", pthread_self());
        if(debug) fprintf(stderr, "SEXSEX22\n");
        fprintf(stderr, "%ld lock 2\n", pthread_self());

        LinkedListNode *request = popHeadList(waitingList);
        if(debug) fprintf(stderr, "SEXSEX99\n");
        fprintf(stderr, "%ld unlock 2\n", pthread_self());

        pthread_mutex_unlock(&waitingList->listMutex);
        fprintf(stderr, "%ld unlock 1\n", pthread_self());
        if(debug) fprintf(stderr, "SEXSEX33\n");

        request->threadID = pthread_self();
        int i =0;
        for (i = 0; i <workers ; ++i) {
            if(threads[i]->threadId == request->threadID){
                break;
            }
        }

        //lock
        /*int i = *((int*)args);
        free(args);*/

        if(debug) fprintf(stderr, "SEXSE444\n");
        fprintf(stderr, "%ld lock 3\n", pthread_self());

        pthread_mutex_lock(&mutex);
        threads[i]->requestCount++;
        threads[i]->arrivalTime = request->arrivalTime;
        gettimeofday(&threads[i]->dispatchTime, NULL);
        if(debug) fprintf(stderr, "SEXSE444444\n");
        fprintf(stderr, "%ld unlock 3\n", pthread_self());

        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "%ld lock 4\n", pthread_self());

        pthread_mutex_lock(&runningList->listMutex);
        pushList(runningList, request->connection);
        fprintf(stderr, "running cond: %d, waiting cond: %d\n", &runningList->emptyCond, &waitingList->emptyCond);
        Stats *tempThread = threads[i];
        fprintf(stderr, "%ld unlock 4\n", pthread_self());

        pthread_mutex_unlock(&runningList->listMutex);


        if(debug) fprintf(stderr, "%d, %ld, %d, >>>>>>>\n", tempThread->index, tempThread->threadId, request->connection);
        requestHandle(request->connection, tempThread);
        close(request->connection);
        if(debug) fprintf(stderr, "aaaaaaaaassss\n");

        LinkedListNode *temp = runningList->head;
        fprintf(stderr, "%ld lock 5\n", pthread_self());

        pthread_mutex_lock(&runningList->listMutex);
        if(debug) fprintf(stderr, "SEXSE445554\n");
        for (int j = 0; j < runningList->size; ++j) {
            if (temp->connection == request->connection) {
                popAtIndexList(runningList, j);
                fprintf(stderr, "j: ---- %d\n", j);

                break;
            }
            temp = temp->next;
        }
        if(debug) fprintf(stderr, "SEXSE4446666\n");

        fprintf(stderr, "%ld unlock 5\n", pthread_self());
        fprintf(stderr, "\n");

        pthread_mutex_unlock(&runningList->listMutex);
    }
    return NULL;
}

bool canBeInserted() {
    pthread_mutex_lock(&waitingList->listMutex);
    pthread_mutex_lock(&runningList->listMutex);
    int sum = waitingList->size + runningList->size;
    pthread_mutex_unlock(&waitingList->listMutex);
    pthread_mutex_unlock(&runningList->listMutex);
    return sum < listSize;
}

void blockPolicy(int connfd) { //todo: cond
    pthread_mutex_lock(&waitingList->listMutex);
    while(!canBeInserted()){
        pthread_cond_wait(&waitingList->fullCond, &waitingList->listMutex);
    }
    pushList(waitingList, connfd);
    pthread_mutex_unlock(&waitingList->listMutex);
}

void dropHeadPolicy(int connfd) {//todo: cond
    fprintf(stderr, "dh 1\n");

    pthread_mutex_lock(&waitingList->listMutex);
    while(waitingList->size == 0){
        fprintf(stderr, "dh 2\n");

        pthread_cond_wait(&waitingList->emptyCond, &waitingList->listMutex);
    }
    fprintf(stderr, "dh 3\n");

    close(popHeadList(waitingList)->connection);
    fprintf(stderr, "dh 4\n");
    //pthread_mutex_unlock(&waitingList->listMutex);
    //pthread_mutex_lock(&waitingList->listMutex);
   /* while(!canBeInserted()){
        pthread_cond_wait(&waitingList->fullCond, &waitingList->listMutex);
    }*/
    pushList(waitingList, connfd);
    fprintf(stderr, "dh 5\n");
    pthread_mutex_unlock(&waitingList->listMutex);
    fprintf(stderr, "dh 6\n");
}

void dropTailPolicy(int connfd) { //todo: cond
    pthread_mutex_lock(&waitingList->listMutex);
    while(waitingList->size == 0){
        pthread_cond_wait(&waitingList->emptyCond, &waitingList->listMutex);
    }
    popTailList(waitingList);
    //pthread_mutex_unlock(&waitingList->listMutex);
    //pthread_mutex_lock(&waitingList->listMutex);
    /*while(!canBeInserted()){
        pthread_cond_wait(&waitingList->fullCond, &waitingList->listMutex);
    }*/
    pushList(waitingList, connfd);
    pthread_mutex_unlock(&waitingList->listMutex);
}

void dropRandomPolicy(int connfd) {
    pthread_mutex_lock(&waitingList->listMutex);
    while(waitingList->size == 0){
        pthread_cond_wait(&waitingList->emptyCond, &waitingList->listMutex);
    }
    //int n = ceil(waitingList->size / 2);
    int n = (waitingList->size + 1) / 2;
    for (int i = 0; i < n; ++i) {
        int r = rand() % waitingList->size;
        popAtIndexList(waitingList, r);
    }
    //pthread_mutex_unlock(&waitingList->listMutex);
    //pthread_mutex_lock(&waitingList->listMutex);
    /*while(!canBeInserted()){
        pthread_cond_wait(&waitingList->fullCond, &waitingList->listMutex);
    }*/
    pushList(waitingList, connfd);
    pthread_mutex_unlock(&waitingList->listMutex);
}

int main(int argc, char *argv[]) {
    int listenfd, connfd, clientlen;
    int port;

    char *schedalg;
    struct sockaddr_in clientaddr;

    pthread_mutex_init(&mutex, NULL);
//    pthread_mutex_init(&runningMutex, NULL);
//    pthread_mutex_init(&waitingMutex, NULL);
//
//    pthread_cond_init(&waitingEmptyCond, NULL);
//    pthread_cond_init(&waitingFullCond, NULL);
//    pthread_cond_init(&runningEmptyCond, NULL);
//    pthread_cond_init(&runningFullCond, NULL);
    waitingList = createList();
    runningList = createList();

    getargs(&port, &workers, &listSize, &schedalg, argc, argv);

    enum SchedalgPolicy policy;
    if (strcmp(schedalg, "block") == SUCCESS) {
        policy = SCHPOL_BLOCK;
    } else if (strcmp(schedalg, "dh") == SUCCESS) {
        policy = SCHPOL_HEAD;
    } else if (strcmp(schedalg, "random") == SUCCESS) {
        policy = SCHPOL_RANDOM;
    } else if (strcmp(schedalg, "dt") == SUCCESS) {
        policy = SCHPOL_TAIL;
    } else {
        exit(0);
    }
    fprintf(stderr, "%d\n", policy);
    threads = (Stats**)malloc(sizeof(Stats*) * workers);
    for (int i = 0; i < workers; ++i) {
        if(debug) fprintf(stderr, "aaaaa\n");
        pthread_t t;
        int *id = (int*)malloc(sizeof(int));
        *id = i;
        if(debug) fprintf(stderr, "%d\n",*id);

        if(pthread_create(&t, NULL, workerRoutine, (void*)(id)) != 0){
            exit(1);
        }
        if(debug) fprintf(stderr, "bbbbb\n");
        threads[i] = malloc(sizeof(Stats));
        threads[i] = createStat(t);
        threads[i]->index = i;
    }
    if(debug) fprintf(stderr, "1\n");

    listenfd = Open_listenfd(port);
    while (1) {
        if(debug) fprintf(stderr, "2\n");

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);

        fprintf(stderr, "waiting: %d, running: %d\n", waitingList->size, runningList->size);

        if(canBeInserted()){
            //pthread_mutex_lock(&waitingList->listMutex);
            pushList(waitingList, connfd);
            //pthread_mutex_unlock(&waitingList->listMutex);
        } else {
            switch (policy) {
                case SCHPOL_BLOCK:
                    blockPolicy(connfd);
                    break;
                case SCHPOL_TAIL:
                    close(connfd);
                    //dropTailPolicy(connfd);
                    break;
                case SCHPOL_HEAD:
                    dropHeadPolicy(connfd);
                    break;
                default:
                    dropRandomPolicy(connfd);
                    break;
            }
        }
        if(debug) fprintf(stderr, "3\n");

        //Close(connfd);
        pthread_cond_broadcast(&waitingList->emptyCond);
    }
    if(debug) fprintf(stderr, "4\n");

}



    


 
