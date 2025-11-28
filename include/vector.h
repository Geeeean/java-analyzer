#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

typedef struct Vector Vector;

Vector* vector_new(size_t element_size);
Vector* vector_new_from_size(size_t size, size_t element_size);
void vector_delete(Vector* v);

int vector_push(Vector* v, void* element);
void* vector_pop(Vector* v);
void* vector_get(const Vector* v, size_t index);

size_t vector_length(const Vector* v);

Vector* vector_copy(const Vector* src);

#endif
