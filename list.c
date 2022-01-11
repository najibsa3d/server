#include "list.h"
#include <stdlib.h>
#include <unistd.h>
#include "stdio.h"



LinkedList* createList(){
    LinkedList* list = malloc(sizeof(*list));
    if(!list)
        return NULL;


    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    pthread_cond_init(&list->fullCond, NULL);
    pthread_cond_init(&list->emptyCond, NULL);
    pthread_mutex_init(&list->listMutex, NULL);

    return list;
}

void pushList(LinkedList* list, int connection){
    if(!list)
        return;

    LinkedListNode* node = malloc(sizeof(*node));
    if(!node)
        return;
    node->connection = connection;
    gettimeofday(&node->arrivalTime, NULL);

    if(!list->head){
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        node->previous = list->tail;
        list->tail = node;
    }
    list->size++;
    //pthread_cond_broadcast(&list->emptyCond);
}

LinkedListNode * popHeadList(LinkedList* list){
    if(!list)
        return NULL;

    LinkedListNode* node;
    if(!list->head)
        return NULL;
    node = list->head;
    list->head = list->head->next;
    if( list->head)
        list->head->previous = NULL;
    list->size--;
    //close(node->connection);
    pthread_cond_broadcast(&list->fullCond);
    return node;
}//todo: close connections

LinkedListNode * popTailList(LinkedList* list){
    if(!list)
        return NULL;

    LinkedListNode* node;
    node = list->tail;
    if(!node)
        return NULL;
    list->tail = list->tail->previous;
    if(list->tail)
        list->tail->next = NULL;
    list->size--;
    pthread_cond_broadcast(&list->fullCond);
    return node;
}

void popAtIndexList(LinkedList* list, int index){
    if(!list)
        return;

    if(index == 0){
        close(popHeadList(list)->connection);
        return;
    }
    else if(index == list->size - 1) {
        close(popTailList(list)->connection);
        return;
    }

    LinkedListNode* temp = list->head;
    for (int i = 0; i < index; ++i) {
        temp = temp->next;
    }
    temp->previous->next = temp->next;
    temp->next->previous = temp->previous;
    list->size--;
    close(temp->connection);
    pthread_cond_broadcast(&list->fullCond);
}
