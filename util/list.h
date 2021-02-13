#ifndef LIST_H
#define LIST_H

struct listElem {
    struct listElem* next;
    struct listElem* prev;
    void* data;
};

struct list {
    struct listElem head;
    struct listElem tail;
};

// List create/destroy
struct list* list_create();
void list_destroy(struct list*);

// List traversal
struct listElem* list_begin(struct list*);
struct listElem* list_next(struct listElem*);
struct listElem* list_end(struct list*);

// List operations
void list_insert(struct list*, struct listElem*, void*);
void* list_remove(struct list*, struct listElem*);

// Queue operations
void queue_push(struct list*, void*);
void* queue_pop(struct list*);
void* queue_peek(struct list*);

// Stack operations
void stack_push(struct list*, void*);
void* stack_pop(struct list*);
void* stack_peek(struct list*);

// Simple operations
int list_isEmpty(struct list*);

#endif