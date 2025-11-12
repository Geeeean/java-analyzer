#include "domain_interval.h"
#include "common.h"
#include "log.h"
#include "opcode.h"
#include "type.h"
#include "vector.h"
#include <limits.h>
#include <stdlib.h>

static Interval interval_top(void)
{
    return (Interval) { .lower = INT_MIN, .upper = INT_MAX };
}

static Interval interval_join_single(Interval a, Interval b)
{
    return (Interval) {
        .lower = (a.lower < b.lower) ? a.lower : b.lower,
        .upper = (a.upper > b.upper) ? a.upper : b.upper
    };
}

static Interval interval_intersect_single(Interval a, Interval b)
{
    return (Interval) {
        .lower = (a.lower > b.lower) ? a.lower : b.lower,
        .upper = (a.upper < b.upper) ? a.upper : b.upper
    };
}

static Interval interval_widen_single(Interval old, Interval newer)
{
    Interval r = old;

    if (newer.lower < old.lower) {
        r.lower = INT_MIN;
    }

    if (newer.upper > old.upper) {
        r.upper = INT_MAX;
    }

    return r;
}

IntervalState* interval_new_top_state(int num_locals)
{
    IntervalState* st = malloc(sizeof(IntervalState));
    if (!st) {
        return NULL;
    }

    st->locals = vector_new(sizeof(Interval));
    st->stack = vector_new(sizeof(Interval));

    for (int i = 0; i < num_locals; i++) {
        Interval iv = interval_top();
        vector_push(st->locals, &iv);
    }

    return st;
}

void interval_state_delete(IntervalState* st)
{
    if (!st) {
        return;
    }

    vector_delete(st->locals);
    vector_delete(st->stack);
    free(st);
}

int interval_state_copy(IntervalState* dst, const IntervalState* src)
{
    if (!dst || !src) {
        return FAILURE;
    }

    if (vector_copy(dst->locals, src->locals)) {
        return FAILURE;
    }

    if (vector_copy(dst->stack, src->stack)) {
        return FAILURE;
    }

    return SUCCESS;
}

int interval_join(IntervalState* acc, const IntervalState* new, int* changed)
{
    if (!acc || !new || !changed) {
        return FAILURE;
    }

    *changed = 0;

    if (vector_length(acc->locals) != vector_length(new->locals)) {
        return FAILURE;
    }

    for (int i = 0; i < vector_length(acc->locals); i++) {
        Interval* a = vector_get(acc->locals, i);
        Interval* b = vector_get(new->locals, i);

        Interval r = interval_join_single(*a, *b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    if (vector_length(acc->stack) != vector_length(new->stack)) {
        /* Stack shape mismatch → collapse to TOP state */
        for (int i = 0; i < vector_length(acc->stack); i++) {
            Interval* s = vector_get(acc->stack, i);
            *s = interval_top();
        }
        *changed = 1;
        return SUCCESS;
    }

    for (int i = 0; i < vector_length(acc->stack); i++) {
        Interval* a = vector_get(acc->stack, i);
        Interval* b = vector_get(new->stack, i);

        Interval r = interval_join_single(*a, *b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_intersection(IntervalState* acc, const IntervalState* constraint, int* changed)
{
    if (!acc || !constraint || !changed) {
        return FAILURE;
    }

    *changed = 0;

    if (vector_length(acc->locals) != vector_length(constraint->locals) || vector_length(acc->stack) != vector_length(constraint->stack)) {
        return FAILURE;
    }

    for (int i = 0; i < vector_length(acc->locals); i++) {
        Interval* a = vector_get(acc->locals, i);
        Interval* b = vector_get(constraint->locals, i);

        Interval r = interval_intersect_single(*a, *b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    for (int i = 0; i < vector_length(acc->stack); i++) {
        Interval* a = vector_get(acc->stack, i);
        Interval* b = vector_get(constraint->stack, i);

        Interval r = interval_intersect_single(*a, *b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_widening(IntervalState* acc, const IntervalState* new, int* changed)
{
    if (!acc || !new || !changed) {
        return FAILURE;
    }

    *changed = 0;

    if (vector_length(acc->locals) != vector_length(new->locals) || vector_length(acc->stack) != vector_length(new->stack)) {
        return FAILURE;
    }

    for (int i = 0; i < vector_length(acc->locals); i++) {
        Interval* a = vector_get(acc->locals, i);
        Interval* b = vector_get(new->locals, i);

        Interval r = interval_widen_single(*a, *b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    for (int i = 0; i < vector_length(acc->stack); i++) {
        Interval* a = vector_get(acc->stack, i);
        Interval* b = vector_get(new->stack, i);

        Interval r = interval_widen_single(*a, *b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    return SUCCESS;
}

// Stack Operations
int interval_stack_push(IntervalState* st, Interval iv)
{
    if (!st || !st->stack) {
        return FAILURE;
    }

    return vector_push(st->stack, &iv);
}

int interval_stack_pop(IntervalState* st, Interval* out)
{
    if (!st || !st->stack) {
        return FAILURE;
    }

    if (vector_pop(st->stack, out)) {
        return FAILURE;
    }

    return SUCCESS;
}

// Abstract arithmetic (stack operands)
Interval interval_add(Interval a, Interval b)
{
    Interval r;
    r.lower = a.lower + b.lower;
    r.upper = a.upper + b.upper;
    return r;
}

Interval interval_sub(Interval a, Interval b)
{
    Interval r;
    r.lower = a.lower - b.upper;
    r.upper = a.upper - b.lower;
    return r;
}

Interval interval_mul(Interval a, Interval b)
{
    int cand[4] = {
        a.lower * b.lower,
        a.lower * b.upper,
        a.upper * b.lower,
        a.upper * b.upper
    };

    Interval r;
    r.lower = cand[0];
    r.upper = cand[0];
    for (int i = 1; i < 4; i++) {
        if (cand[i] < r.lower) {
            r.lower = cand[i];
        }
        if (cand[i] > r.upper) {
            r.upper = cand[i];
        }
    }
    return r;
}

Interval interval_div(Interval a, Interval b)
{
    if (b.lower <= 0 && b.upper >= 0) {
        return interval_top();
    }

    int cand[4] = {
        a.lower / b.lower,
        a.lower / b.upper,
        a.upper / b.lower,
        a.upper / b.upper
    };

    Interval r;
    r.lower = cand[0];
    r.upper = cand[0];
    for (int i = 1; i < 4; i++) {
        if (cand[i] < r.lower) {
            r.lower = cand[i];
        }
        if (cand[i] > r.upper) {
            r.upper = cand[i];
        }
    }
    return r;
}

static int handle_push(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    PushOP* push = &ir_instruction->data.push;
    if (!push) {
        return FAILURE;
    }

    int value;
    if (push->value.type == TYPE_INT) {
        value = push->value.data.int_value;
    } else if (push->value.type == TYPE_BOOLEAN) {
        value = push->value.data.bool_value;
    } else if (push->value.type == TYPE_CHAR) {
        value = push->value.data.char_value;
    } else {
        return FAILURE;
    }

    if (interval_stack_push(out_state, (Interval) { .lower = value, .upper = value })) {
        return FAILURE;
    }

    return SUCCESS;
}

static int handle_load(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    LoadOP* load = &ir_instruction->data.load;
    if (!load) {
        return FAILURE;
    }

    Interval* interval = vector_get(out_state->locals, load->index);
    if (vector_push(out_state->stack, interval)) {
        return FAILURE;
    }

    return SUCCESS;
}

static int handle_store(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    StoreOP* store = &ir_instruction->data.store;
    if (!store) {
        return FAILURE;
    }

    Interval interval;
    if (interval_stack_pop(out_state, &interval)) {
        return FAILURE;
    }

    Interval* local_interval = vector_get(out_state->locals, store->index);
    if (!local_interval) {
        return FAILURE;
    }

    local_interval->lower = interval.lower;
    local_interval->upper = interval.upper;

    return SUCCESS;
}

static int handle_dup(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    Interval interval;
    if (interval_stack_pop(out_state, &interval)) {
        return FAILURE;
    }

    if (interval_stack_push(out_state, interval)) {
        return FAILURE;
    }
    if (interval_stack_push(out_state, interval)) {
        return FAILURE;
    }

    return SUCCESS;
}

static int handle_binary(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    BinaryOP* binary = &ir_instruction->data.binary;
    if (!binary) {
        return FAILURE;
    }

    Interval interval1, interval2, result;
    if (interval_stack_pop(out_state, &interval1)) {
        return FAILURE;
    }
    if (interval_stack_pop(out_state, &interval2)) {
        return FAILURE;
    }

    switch (binary->op) {
    case BO_MUL:
        result = interval_mul(interval2, interval1);
        break;

    case BO_ADD:
        result = interval_add(interval2, interval1);
        break;

    case BO_DIV:
        result = interval_div(interval2, interval1);
        break;

    case BO_SUB:
        result = interval_sub(interval2, interval1);
        break;

    // case BO_REM:
    //     break;
    default:
        LOG_ERROR("While abstract binary op");
        return FAILURE;
    }

    if (interval_stack_push(out_state, result)) {
        return FAILURE;
    }

    return SUCCESS;
}

int interval_transfer(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    int result = SUCCESS;

    switch (ir_instruction->opcode) {
    case OP_PUSH:
        result = handle_push(out_state, ir_instruction);
        break;
    case OP_LOAD:
        result = handle_load(out_state, ir_instruction);
        break;
    case OP_STORE:
        result = handle_store(out_state, ir_instruction);
        break;
    case OP_DUP:
        result = handle_dup(out_state, ir_instruction);
        break;
    case OP_BINARY:
        result = handle_binary(out_state, ir_instruction);
        break;
    case OP_ARRAY_STORE:
    case OP_NEW_ARRAY:
    case OP_ARRAY_LOAD:
    case OP_ARRAY_LENGTH:
        result = FAILURE;
        break;
    default:
        break;
    }

    if (result) {
        LOG_ERROR("%s", opcode_print(ir_instruction->opcode));
    }

    return SUCCESS;
}

void interval_state_print(const IntervalState* st)
{
    if (!st) {
        LOG_INFO("(null)");
        return;
    }

    int num_locals = vector_length(st->locals);
    int num_stack = vector_length(st->stack);

    LOG_INFO("Locals: ");
    for (int i = 0; i < num_locals; i++) {
        Interval* iv = vector_get(st->locals, i);
        if (iv->lower == INT_MIN && iv->upper == INT_MAX) {
            LOG_INFO("v%d=[⊤]", i);
        } else {
            LOG_INFO("v%d=[%d,%d]", i, iv->lower, iv->upper);
        }
    }

    if (num_stack > 0) {
        LOG_INFO("Stack: ");
        for (int i = 0; i < num_stack; i++) {
            Interval* iv = vector_get(st->stack, i);
            if (iv->lower == INT_MIN && iv->upper == INT_MAX) {
                LOG_INFO("[%d:⊤]", i);
            } else {
                LOG_INFO("[%d:%d,%d]", i, iv->lower, iv->upper);
            }
        }
    }
}
