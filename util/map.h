#ifndef MAP_H
#define MAP_H

#include "./list.h"

struct mapNode {
	char* key;
	void* value;
	struct mapNode* next;
};

// Maps text to void pointers
struct map {
	int size;
	int capacity;
	struct mapNode** lists;
	struct list* keyList;
};

struct map* map_create();
void map_destroy(struct map*);
int map_put(struct map*, char*, void*);
void* map_get(struct map*, const char*);
struct list* map_getKeyList(struct map* map);

#endif