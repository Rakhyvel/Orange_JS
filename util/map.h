#ifndef MAP_H
#define MAP_H

struct mapNode {
	char* key;
	void* value;
	struct mapNode* next;
};

// Maps void pointers to void pointers
struct map {
	int size;
	int capacity;
	struct mapNode** lists;
};

struct map* map_init();
void map_destroy(struct map*);
void map_put(struct map*, char*, void*);
void* map_get(struct map*, const char*);

#endif