#ifndef STACK_H
#define STACK_H

#include "type.h"

#include <stdbool.h>

typedef struct Stack Stack;

Stack* stack_new();
void stack_delete(Stack* stack);

PrimitiveType* stack_peek(Stack* stack);
int stack_pop(Stack* stack, PrimitiveType* value);
int stack_push(Stack* stack, PrimitiveType value);
// bool stack_same_type_on_top(Stack* stack);

#endif
