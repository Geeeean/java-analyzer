#include "vector.h"
#include "common.h"
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

void vector_reverse(Vector* v)
{
    if (!v || v->length <= 1) {
        return;
    }

    size_t elem_size = v->element_size;
    char* data = (char*)v->data;

    for (int i = 0; i < v->length / 2; i++) {
        int j = v->length - 1 - i;

        char tmp[elem_size];
        memcpy(tmp, data + i * elem_size, elem_size);
        memcpy(data + i * elem_size, data + j * elem_size, elem_size);
        memcpy(data + j * elem_size, tmp, elem_size);
    }
}

int vector_copy(Vector* dst, const Vector* src)
{
    if (!dst || !src) {
        return FAILURE;
    }

    if (dst->capacity < src->length) {
        void* new_data = realloc(dst->data, src->element_size * src->capacity);
        if (!new_data) {
            return FAILURE;
        }
        dst->data = new_data;
        dst->capacity = src->capacity;
    }

    dst->element_size = src->element_size;
    dst->length = src->length;

    memcpy(dst->data, src->data, src->length * src->element_size);

    return SUCCESS;
}
