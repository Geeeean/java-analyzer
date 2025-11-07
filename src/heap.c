#include "heap.h"
#include <stdlib.h>

#define HEAP_SIZE 100

// todo make dynamic
struct Heap {
    ObjectValue* fields[HEAP_SIZE];
    int len;
};

Heap* heap_create()
{
    Heap* heap = malloc(sizeof(Heap));
    if (heap) {
        heap->len = 0;
    }

    return heap;
}

int heap_insert(Heap* heap, ObjectValue* obj, int* index)
{
    if (heap->len >= HEAP_SIZE) {
        return 1;
    }

    *index = heap->len;
    heap->fields[heap->len] = obj;
    heap->len++;

    return 0;
}

ObjectValue* heap_get(Heap* heap, int index)
{
    if (index < 0 || index >= heap->len) {
        return NULL;
    }

    return heap->fields[index];
}

int heap_length(Heap* heap)
{
    return heap->len;
}
