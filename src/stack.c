#include "stack.h"

#include <stdlib.h>

typedef struct Item Item;
struct Item {
    Value value;
    Item* next;
};

struct Stack {
    int size;
    Item* head;
};

int stack_push(Stack* stack, Value value)
{
    Item* to_push = malloc(sizeof(Value));
    if (!to_push) {
        return 1;
    }

    stack->size += 1;

    to_push->value = value;
    to_push->next = stack->head;

    stack->head = to_push;

    return 0;
}

Value* stack_peek(Stack* stack)
{
    if (stack->size == 0) {
        return NULL;
    }

    return &stack->head->value;
}

int stack_pop(Stack* stack)
{
    if (stack->size == 0) {
        return 1;
    }

    Item* to_pop = stack->head;
    stack->head = to_pop->next;

    free(to_pop);

    return 0;
}

bool stack_same_type_on_top(Stack* stack)
{
    if (stack->size < 2) {
        return false;
    }

    ValueType type1 = stack->head->value.type;
    ValueType type2 = stack->head->next->value.type;

    return type1 == type2;
}

Stack* stack_new() {
    Stack* stack = malloc(sizeof(Stack));
    if (!stack) {
        return NULL;
    }

    stack->head = NULL;
    stack->size = 0;

    return stack;
}

void stack_delete(Stack* stack) {
    if (!stack) {
        return;
    }

    while (stack->head) {
        stack_pop(stack);
    }

    free(stack);
}
