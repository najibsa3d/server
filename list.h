#ifndef OS_HW3_LIST_H
#define OS_HW3_LIST_H
#include <pthread.h>
#include <sys/time.h>

typedef struct LinkedListNode_t {
    pthread_t threadID;
    struct timeval arrivalTime;
    struct timeval dispatchTime;
    int connection;
    struct LinkedListNode_t* next;
    struct LinkedListNode_t* previous;
} LinkedListNode;



typedef struct LinkedList_t {
    LinkedListNode* head;
    LinkedListNode* tail;
    int size;
    pthread_mutex_t listMutex;
    pthread_cond_t fullCond;
    pthread_cond_t emptyCond;
} LinkedList;

LinkedList* createList();
void pushList(LinkedList* list, int connection);
LinkedListNode* popHeadList(LinkedList* list);
LinkedListNode* popTailList(LinkedList* list);
void popAtIndexList(LinkedList* list, int index);
#endif //OS_HW3_LIST_H
