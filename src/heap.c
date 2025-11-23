#include "heap.h"
#include <stdlib.h>

#define HEAP_SIZE 100000

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

void heap_reset(void)
{
    for (int i = 1; i < heap.len; i++) {
        ObjectValue* obj = heap.fields[i];
        if (!obj) continue;

        if (obj->type && obj->type->kind == TK_ARRAY) {
            free(obj->array.elements);
            obj->array.elements = NULL;
            obj->array.elements_count = 0;
        }

        free(obj);
        heap.fields[i] = NULL;
    }

    heap.len = 1;
}