#ifndef STACK_H
#define STACK_H

#include "value.h"

#include <stdbool.h>

typedef struct Stack Stack;

Stack* stack_new();
void stack_delete(Stack* stack);

Value* stack_peek(Stack* stack);
int stack_pop(Stack* stack, Value* value);
int stack_push(Stack* stack, Value value);
// bool stack_same_type_on_top(Stack* stack);

void stack_clear(Stack* stack);

#endif

