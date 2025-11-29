#ifndef HEAP_H
#define HEAP_H

#include "value.h"
#define HEAP_SIZE 100000

typedef struct {
  ObjectValue* fields[HEAP_SIZE];
  int len;
} Heap;

Heap* heap_create();
void heap_free(Heap* h);

// Per-heap operations:
void heap_init(Heap* h);
void heap_reset(Heap* h);
int  heap_insert(Heap* h, ObjectValue* obj, int* index);
ObjectValue* heap_get(Heap* h, int index);
int heap_insert(Heap* heap, ObjectValue* obj, int* index);
ObjectValue* heap_get(Heap* heap, int index);
int heap_length(Heap* heap);

#endif