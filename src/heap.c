#include "heap.h"
#include <stdlib.h>

Heap* heap_create() {
    Heap* h = malloc(sizeof(Heap));
    if (!h) return NULL;
    heap_init(h);
    return h;
}

void heap_free(Heap* h) {
    if (!h) return;
// todo make dynamic
struct Heap {
    ObjectValue* fields[HEAP_SIZE];
    int len;
};

    heap_reset(h);
    free(h);
}

void heap_init(Heap* h) {
    h->len = 1;
    for (int i = 0; i < HEAP_SIZE; i++)
        h->fields[i] = NULL;
}

int heap_insert(Heap* h, ObjectValue* obj, int* index)
{
    if (h->len >= HEAP_SIZE)
        return 1;

    *index = h->len;
    h->fields[h->len] = obj;
    h->len++;
    return 0;
}

ObjectValue* heap_get(Heap* h, int index)
{
    if (index < 0 || index >= h->len)
        return NULL;
    return h->fields[index];
}

void heap_reset(Heap* h)
{
    for (int i = 1; i < h->len; i++) {
        ObjectValue* obj = h->fields[i];
        if (!obj) continue;

        if (obj->type && obj->type->kind == TK_ARRAY) {
            free(obj->array.elements);
        }

        free(obj);
        h->fields[i] = NULL;
    }

    h->len = 1;
}
int heap_length(Heap* heap)
{
    return heap->len;
}
