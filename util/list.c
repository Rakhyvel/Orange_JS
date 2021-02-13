/*
    A doubly linked list is represented by list elements that have pointers to
    the next and previous list element.

    List structs have a head and tail list element that do not store data, and 
    only serve to mark out the begining and end of a list. 

    Linked lists can represent queues, stacks, or regular lists */

#include <stdlib.h>
#include <stdio.h>

#include "list.h"
#include "../debug.h"

/*  Creates a new list, with no new nodes. */
struct list* list_create() {
    struct list* list = (struct list*)malloc(sizeof(struct list));

    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
    
    return list;
}

/*  Destroys a list, and all of it's list elements. 

    WARNING! The data pointed to by the list will NOT be freed. Make sure to
    free data before hand! (or it'll be bad) */
void list_destroy(struct list* list) {
    ASSERT(list != NULL);
}

/*  Gives the starting point of the list */
struct listElem* list_begin(struct list* list) {
    ASSERT(list != NULL);
    return list->head.next;
}

/*  Returns the next list element */
struct listElem* list_next(struct listElem* elem) {
    ASSERT(elem != NULL);
    return elem->next;
}

/*  Returns the tail of the linked list, can be used to detect when a loop has
    reached the end of the list during traversal */
struct listElem* list_end(struct list* list) {
    ASSERT(list != NULL);
    return &list->tail;
}

/*
    Inserts a new list element after a given list element. */
void list_insert(struct list* list, struct listElem* before, void* data) {
    ASSERT(list != NULL);
    ASSERT(before != NULL);
    struct listElem* elem = (struct listElem*)malloc(sizeof(struct listElem));
    elem->data = data;
    elem->prev = before->prev;
    elem->next = before;
    before->prev->next = elem;
    before->prev = elem;
}

/*
    Removes a given list element from a list. Returns the data that used to be
    at the old list element */
void* list_remove(struct list* list, struct listElem* elem) {
    ASSERT(list != NULL);
    ASSERT(!list_isEmpty(list));
    ASSERT(elem != &list->head);
    ASSERT(elem != &list->tail);

    struct listElem* prev = elem->prev;
    struct listElem* next = elem->next;
    void* retval = elem->data;

    // Swap pointers, free old node
    prev->next = next;
    next->prev = prev;
    free(elem);

    return retval;
}

/* Removes the first element in the list, returns data pointer */
void* list_pop(struct list* list) {
    ASSERT(list != NULL);
    ASSERT(!list_isEmpty(list));
    return list_remove(list, list_begin(list));
}

/* Returns data pointer of the first element in the list */
void* list_front(struct list* list) {
    ASSERT(list != NULL);
    ASSERT(!list_isEmpty(list));
    return list_begin(list)->data;
}

/*  Appends a listElem to the end of the queue, with the given data 
    pointer */
void queue_push(struct list* list, void* data) {
    list_insert(list, list_end(list), data);
}

/*
    Removes the front element from the queue, returns the pointer held by that 
    first element */
void* queue_pop(struct list* list) {
    ASSERT(!list_isEmpty(list));
    return list_pop(list);
}
/*
    Returns the data pointer of the front of the queue */
void* queue_peek(struct list* list) {
    ASSERT(!list_isEmpty(list));
    return list_front(list);
}

/* Inserts an element at the top of a stack */
void stack_push(struct list* list, void* data) {
    ASSERT(list != NULL);
    list_insert(list, list_begin(list), data);
}

/* Removes the top element from the stack, returns the data pointer */
void* stack_pop(struct list* list) {
    return list_pop(list);
}

/*
    Returns the data pointer of the element at the top of the stack */
void* stack_peek(struct list* list) {
    return list_front(list);
}

/* 
    Returns true if the list head points directly to the list tail */
int list_isEmpty(struct list* list) {
    ASSERT(list != NULL);

    return list_begin(list) == list_end(list);
}