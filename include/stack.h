#ifndef STACK_H
#define STACK_H

#include "decompiled_parser.h"

#include <stdbool.h>

typedef struct Stack Stack;

Value* stack_peek(Stack* stack);
int stack_pop(Stack* stack);
int stack_push(Stack* stack, Value value);
bool stack_same_type_on_top(Stack* stack);

#endif
