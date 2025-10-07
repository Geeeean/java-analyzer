#include "heap.h"
#include <stdlib.h>

#define HEAP_SIZE 100

typedef struct {
    ObjectValue* fields[HEAP_SIZE];
    int len;
} Heap;

static Heap heap = { .len = 1, .fields = { NULL } };

int heap_insert(ObjectValue* obj, int* index)
{
    if (heap.len >= HEAP_SIZE) {
        return 1;
    }

    *index = heap.len;

    heap.fields[heap.len] = obj;

    heap.len++;

    return 0;
}

ObjectValue* heap_get(int index)
{
    if (index < 0 || index >= heap.len) {
        return NULL;
    }

    return heap.fields[index];
}
