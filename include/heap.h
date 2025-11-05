#ifndef HEAP_H
#define HEAP_H

#include "value.h"

typedef struct Heap Heap;

Heap* heap_create();
int heap_insert(Heap* heap, ObjectValue* obj, int* index);
ObjectValue* heap_get(Heap* heap, int index);

#endif
