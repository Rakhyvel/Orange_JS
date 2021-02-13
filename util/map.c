/*
    Maps are used to associate string names with pieces of data. They can give 
    almost instantaneous lookup capabilities. */

#include "map.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../debug.h"

int hash(const char*);

/*
    Creates a map pointer */
struct map* map_init() {
    struct map* map = (struct map*)malloc(sizeof(map));
    map->size = 0;
    map->capacity = 10;
    map->lists = (struct mapNode**)malloc(map->capacity * sizeof(struct mapNode*));
    for(int i = 0; i < map->capacity; i++) {
        map->lists[i] = NULL;
    }
    return map;
}

/*
    Destroys a map pointer.
    
    WARNING! The data pointed to by the map will NOT be freed. Make sure to
    free data before hand! (or it'll be bad)  */
void map_destroy(struct map* map) {
    ASSERT(map != NULL);
    for (int i = 0; i < map->capacity; i++) {
        if (!map->lists[i]) {
            struct mapNode* curr = map->lists[i];
            while (curr != NULL) {
                struct mapNode* freeMe = curr;
                curr = curr->next;
                free(freeMe);
            }
        }
    }
    free(map);
}

/*
    Associates a string value with a pointer value in the map */
void map_put(struct map* map, char* key, void* value) {
    ASSERT(map != NULL);
    ASSERT(key != NULL);
    unsigned int hashcode = abs(hash(key)) % map->capacity;
    struct mapNode* bucket = map->lists[hashcode];
    if (bucket == NULL) {
        map->lists[hashcode] = (struct mapNode*)malloc(sizeof(struct mapNode));
        map->lists[hashcode]->key = key;
        map->lists[hashcode]->value = value;
        map->lists[hashcode]->next = NULL;
    } else {
        // Bucket cannot be null
        struct mapNode* curr = bucket;
        int contains = 0;
        while (curr->next != NULL) {
            if (strcmp(curr->key, key) == 1) {
                contains = 1;
                break;
            }
            curr = curr->next;
        }
        if (!contains) {
            curr->next = (struct mapNode*)malloc(sizeof(struct mapNode));
            map->lists[hashcode]->key = key;
            curr->next->value = value;
            curr->next->next = NULL;
        }
    }
    map->size++;
}

/*
    Returns a the pointer associated with a given string key */
void* map_get(struct map* map, const char* key) {
    ASSERT(map != NULL);
    ASSERT(key != NULL);
    unsigned int hashcode = abs(hash(key)) % map->capacity;
    struct mapNode* bucket = map->lists[hashcode];
    if (bucket == NULL) {
        return NULL;
    }
    else {
        struct mapNode* curr = bucket;
        while (curr != NULL) {
            if (strcmp(curr->key, key) == 0) {
                return curr->value;
            }
            curr = curr->next;
        }
    }
    return NULL;
}

/*
    Function that converts every string to an almost unique number.
    
    Shamelessly copied from somewhere on StackOverflow */
int hash(const char* str) {
    ASSERT(str != NULL);
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = hash * 33 + c;

    return hash;
}