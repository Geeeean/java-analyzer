#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

typedef struct Vector Vector;

Vector* vector_new(size_t element_size);
Vector* vector_new_from_size(size_t size, size_t element_size);
void vector_delete(Vector* v);

int vector_push(Vector* v, void* element);
int vector_pop(Vector* v, void* out);
void* vector_get(const Vector* v, size_t index);
void vector_reverse(Vector* v);
int vector_copy(Vector* dst, const Vector* src);

size_t vector_length(const Vector* v);

#endif
