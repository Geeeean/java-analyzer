#include "vector.h"
#include "log.h"

#include <string.h>

#define START_SIZE 8

struct Vector {
    size_t length;
    size_t capacity;
    size_t element_size;
    void* data;
};

Vector* vector_new_from_size(size_t size, size_t element_size)
{
    Vector* v = malloc(sizeof(Vector));
    if (!v) {
        return NULL;
    }

    v->length = 0;
    v->capacity = size;
    v->element_size = element_size;
    v->data = malloc(element_size * size);

    return v;
}

Vector* vector_new(size_t element_size)
{
    return vector_new_from_size(START_SIZE, element_size);
}

void vector_delete(Vector* v)
{
    if (v) {
        free(v->data);
    }

    free(v);
}

int vector_push(Vector* v, void* element)
{
    if (!v) {
        return 1;
    }

    if (v->length >= v->capacity) {
        if (!v->capacity) {
            v->capacity = START_SIZE;
        } else {
            v->capacity *= 2;
        }

        void* tmp = realloc(v->data, v->element_size * v->capacity);
        if (!tmp) {
            return 2;
        }

        v->data = tmp;
    }

    void* address = (char*)v->data + v->length * v->element_size;
    memcpy(address, element, v->element_size);

    v->length++;

    return 0;
}

void* vector_get(const Vector* v, size_t index)
{
    if (index >= v->length) {
        return NULL;
    }

    return (char*)v->data + index * v->element_size;
}

void* vector_pop(Vector* v)
{
    LOG_ERROR("TODO: Vector pop");
    return NULL;
}

size_t vector_length(const Vector* v)
{
    return v->length;
}
