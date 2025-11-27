#include "stack.h"

#include <stdlib.h>

typedef struct Item Item;
struct Item {
    Value value;
    Item* next;
};

struct Stack {
    int size;
    Item* top;
};

int stack_push(Stack* stack, Value value)
{
    Item* to_push = malloc(sizeof(Item));
    if (!to_push) {
        return 1;
    }

    stack->size += 1;

    to_push->value = value;
    to_push->next = stack->top;

    stack->top = to_push;

    return 0;
}

Value* stack_peek(Stack* stack)
{
    if (stack->size == 0) {
        return NULL;
    }

    return &stack->top->value;
}

int stack_pop(Stack* stack, Value* value)
{
    if (stack->size == 0) {
        value->type = TYPE_VOID;
        return 1;
    }

    Item* to_pop = stack->top;
    stack->top = to_pop->next;

    if (value) {
        *value = to_pop->value;
    }

    stack->size--;

    free(to_pop);
    return 0;
}

static bool stack_same_type_on_top(Stack* stack)
{
    if (stack->size < 2) {
        return false;
    }

    Type* type1 = stack->top->value.type;
    Type* type2 = stack->top->next->value.type;

    return type1 == type2;
}

Stack* stack_new()
{
    Stack* stack = malloc(sizeof(Stack));
    if (!stack) {
        return NULL;
    }

    stack->top = NULL;
    stack->size = 0;

    return stack;
}

void stack_delete(Stack* stack)
{
    if (!stack) {
        return;
    }

    while (stack->top) {
        stack_pop(stack, NULL);
    }

    free(stack);
}

void stack_clear(Stack* stack)
{
    if (!stack) return;

    while (stack->top) {
        stack_pop(stack, NULL);
    }

    stack->size = 0;
}