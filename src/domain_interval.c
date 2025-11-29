#include "domain_interval.h"
#include "common.h"
#include "log.h"
#include "opcode.h"
#include "type.h"
#include "vector.h"
#include <limits.h>
#include <stdlib.h>

#define MAKE_INTERVAL(out, L, U) \
    do {                         \
        if ((L) <= (U)) {        \
            (out)->lower = (L);  \
            (out)->upper = (U);  \
        } else {                 \
            *(out) = bottom;     \
        }                        \
    } while (0)

static Interval interval_top(void)
{
    return (Interval) { .lower = INT_MIN, .upper = INT_MAX };
}

static Interval interval_bottom(void)
{
    return (Interval) { .lower = 1, .upper = -1 };
}

static int is_interval_bottom(Interval iv)
{
    Interval bottom = interval_bottom();
    return iv.lower == bottom.lower && iv.upper == bottom.upper;
}

static Interval interval_join_single(Interval a, Interval b)
{
    if (is_interval_bottom(a)) {
        return b;
    } else if (is_interval_bottom(b)) {
        return a;
    } else {
        return (Interval) {
            .lower = (a.lower < b.lower) ? a.lower : b.lower,
            .upper = (a.upper > b.upper) ? a.upper : b.upper
        };
    }
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

    r.lower = MIN(r.lower, MIN(newer.lower, old.lower));

    LOG_DEBUG("NEW UPPER %d", newer.upper);
    LOG_DEBUG("OLD UPPER %d", old.upper);

    if (newer.upper > old.upper) {
        r.upper = INT_MAX;
    }

    r.upper = MAX(r.upper, MAX(newer.upper, old.upper));

    return r;
}

IntervalState* interval_new_top_state(int num_locals)
{
    IntervalState* st = malloc(sizeof(IntervalState));
    if (!st) {
        return NULL;
    }

    st->locals = vector_new(sizeof(int));
    st->stack  = vector_new(sizeof(int));
    st->env    = vector_new(sizeof(Interval));

    if (!st->locals || !st->stack || !st->env) {
        interval_state_delete(st);
        return NULL;
    }

    for (int i = 0; i < num_locals; i++) {
        vector_push(st->locals, &i);

        Interval iv = interval_top();
        vector_push(st->env, &iv);
    }

    return st;
}

IntervalState* interval_new_bottom_state(int num_locals)
{
    IntervalState* st = malloc(sizeof(IntervalState));
    if (!st) {
        return NULL;
    }

    st->locals = vector_new(sizeof(int));
    st->stack  = vector_new(sizeof(int));
    st->env    = vector_new(sizeof(Interval));

    if (!st->locals || !st->stack || !st->env) {
        interval_state_delete(st);
        return NULL;
    }

    for (int i = 0; i < num_locals; i++) {
        vector_push(st->locals, &i);

        Interval iv = interval_bottom();
        vector_push(st->env, &iv);
    }

    return st;
}

void interval_state_delete(IntervalState* st)
{
    if (!st) {
        return;
    }

    if (st->env)    vector_delete(st->env);
    if (st->locals) vector_delete(st->locals);
    if (st->stack)  vector_delete(st->stack);
    free(st);
}

/*
 * Deep copy: dst := src
 * Rebuild dst->locals, dst->stack, dst->env from src using vector_{length,get,push}.
 * This avoids depending on vector_copy’s signature.
 */
int interval_state_copy(IntervalState* dst, const IntervalState* src)
{
    if (!dst || !src) {
        return FAILURE;
    }

    // Drop old vectors (if any)
    if (dst->locals) vector_delete(dst->locals);
    if (dst->stack)  vector_delete(dst->stack);
    if (dst->env)    vector_delete(dst->env);

    dst->locals = vector_new(sizeof(int));
    dst->stack  = vector_new(sizeof(int));
    dst->env    = vector_new(sizeof(Interval));

    if (!dst->locals || !dst->stack || !dst->env) {
        return FAILURE;
    }

    // Copy env
    int env_len = vector_length(src->env);
    for (int i = 0; i < env_len; i++) {
        Interval* iv = vector_get(src->env, i);
        if (!iv) return FAILURE;
        if (vector_push(dst->env, iv)) return FAILURE;
    }

    // Copy locals
    int locals_len = vector_length(src->locals);
    for (int i = 0; i < locals_len; i++) {
        int* name = vector_get(src->locals, i);
        if (!name) return FAILURE;
        if (vector_push(dst->locals, name)) return FAILURE;
    }

    // Copy stack
    int stack_len = vector_length(src->stack);
    for (int i = 0; i < stack_len; i++) {
        int* name = vector_get(src->stack, i);
        if (!name) return FAILURE;
        if (vector_push(dst->stack, name)) return FAILURE;
    }

    return SUCCESS;
}

int interval_join(IntervalState* acc, const IntervalState* new, int* changed)
{
    if (!acc || !new || !changed) {
        return FAILURE;
    }

    int new_env_len = vector_length(new->env);

    while (vector_length(acc->env) < new_env_len) {
        Interval bottom = interval_bottom();
        vector_push(acc->env, &bottom);
        *changed = 1;
    }

    for (int i = 0; i < new_env_len; i++) {
        Interval* a = vector_get(acc->env, i);
        Interval b  = *(Interval*)vector_get(new->env, i);

        Interval r = interval_join_single(*a, b);

        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    int locals_len = MIN(vector_length(acc->locals), vector_length(new->locals));

    for (int i = 0; i < locals_len; i++) {
        int* nameA = (int*)vector_get(acc->locals, i);
        int nameB  = *(int*)vector_get(new->locals, i);

        if (*nameA == nameB)
            continue;

        int new_name = vector_length(acc->env);

        Interval a = *(Interval*)vector_get(acc->env, *nameA);
        Interval b = *(Interval*)vector_get(new->env, nameB);

        Interval r = interval_join_single(a, b);

        vector_push(acc->env, &r);

        *nameA = new_name;
        *changed = 1;
    }

    for (int i = locals_len; i < vector_length(new->locals); i++) {
        vector_push(acc->locals, vector_get(new->locals, i));
    }

    int lenA = vector_length(acc->stack);
    int lenB = vector_length(new->stack);

    for (int i = 0; i < MAX(lenA, lenB); i++) {
        int* nameA = vector_get(acc->stack, i);
        int* nameB = vector_get(new->stack, i);

        if (nameA && nameB) {
            if (*nameA != *nameB) {
                int new_name = vector_length(acc->env);

                Interval a = *(Interval*)vector_get(acc->env, *nameA);
                Interval b = *(Interval*)vector_get(new->env, *nameB);
                Interval r = interval_join_single(a, b);

                vector_push(acc->env, &r);
                *nameA = new_name;
                *changed = 1;
            }
        } else {
            Interval a = nameA ? *(Interval*)vector_get(acc->env, *nameA) : interval_bottom();
            Interval b = nameB ? *(Interval*)vector_get(new->env, *nameB) : interval_bottom();
            Interval r = interval_join_single(a, b);

            vector_push(acc->env, &r);
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_intersection(IntervalState* acc, const IntervalState* constraint, int* changed)
{
    if (!acc || !constraint || !changed)
        return FAILURE;
    *changed = 0;

    int env_len = vector_length(acc->env);
    int c_len   = vector_length(constraint->env);
    int max_len = (env_len > c_len ? env_len : c_len);

    while (vector_length(acc->env) < max_len) {
        Interval top = interval_top();
        vector_push(acc->env, &top);
    }

    for (int i = 0; i < max_len; i++) {
        Interval* a = vector_get(acc->env, i);

        Interval b = interval_top();
        if (i < c_len)
            b = *(Interval*)vector_get(constraint->env, i);

        Interval r = interval_intersect_single(*a, b);
        if (r.lower != a->lower || r.upper != a->upper) {
            *a = r;
            *changed = 1;
        }
    }

    for (int i = 0; i < vector_length(acc->locals); i++) {
        int* nameA = vector_get(acc->locals, i);
        int nameB  = *(int*)vector_get(constraint->locals, i);

        if (*nameA != nameB) {
            int new_name = vector_length(acc->env);

            Interval a = *(Interval*)vector_get(acc->env, *nameA);
            Interval b = *(Interval*)vector_get(constraint->env, nameB);
            Interval r = interval_intersect_single(a, b);

            vector_push(acc->env, &r);
            *nameA = new_name;
            *changed = 1;
        }
    }

    for (int i = 0; i < vector_length(acc->stack); i++) {
        int* nameA = vector_get(acc->stack, i);
        int nameB  = *(int*)vector_get(constraint->stack, i);

        if (*nameA != nameB) {
            int new_name = vector_length(acc->env);

            Interval a = *(Interval*)vector_get(acc->env, *nameA);
            Interval b = *(Interval*)vector_get(constraint->env, nameB);
            Interval r = interval_intersect_single(a, b);

            vector_push(acc->env, &r);
            *nameA = new_name;
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_widening(IntervalState* acc, const IntervalState* new, int* changed)
{
    if (!acc || !new || !changed)
        return FAILURE;
    *changed = 0;

    if (vector_length(new->locals) != vector_length(acc->locals)) {
        LOG_ERROR("solve in interval widening");
    }

    for (int i = 0; i < vector_length(new->locals); i++) {
        int in_iv_id  = *(int*)vector_get(acc->locals, i);
        int new_iv_id = *(int*)vector_get(new->locals, i);

        Interval* in_iv  = vector_get(acc->env, in_iv_id);
        Interval* new_iv = vector_get(new->env, new_iv_id);

        Interval r = interval_widen_single(*in_iv, *new_iv);

        LOG_DEBUG("AAAAAAAAAAAAAAAAAAAAAA");
        if (r.lower != new_iv->lower || r.upper != new_iv->upper) {
            LOG_DEBUG("UA BOSS");
            int* a = vector_get(acc->locals, i);
            *a = vector_length(acc->env);
            vector_push(acc->env, &r);

            *changed = 1;
        }
    }

    return SUCCESS;
}

// Abstract arithmetic (stack operands)
Interval interval_add(Interval* a, Interval* b)
{
    Interval r;
    r.lower = a->lower + b->lower;
    r.upper = a->upper + b->upper;
    return r;
}

Interval interval_sub(const Interval* a, const Interval* b)
{
    Interval r;

    long tmp_lower = (long)a->lower - (long)b->upper;
    long tmp_upper = (long)a->upper - (long)b->lower;

    if (tmp_lower < INT_MIN)
        tmp_lower = INT_MIN;
    if (tmp_lower > INT_MAX)
        tmp_lower = INT_MAX;

    if (tmp_upper < INT_MIN)
        tmp_upper = INT_MIN;
    if (tmp_upper > INT_MAX)
        tmp_upper = INT_MAX;

    r.lower = (int)tmp_lower;
    r.upper = (int)tmp_upper;

    return r;
}

Interval interval_mul(Interval* a, Interval* b)
{
    int cand[4] = {
        a->lower * b->lower,
        a->lower * b->upper,
        a->upper * b->lower,
        a->upper * b->upper
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

Interval interval_div(Interval* a, Interval* b)
{
    if (b->lower <= 0 && b->upper >= 0) {
        return interval_top();
    }

    int cand[4] = {
        a->lower / b->lower,
        a->lower / b->upper,
        a->upper / b->lower,
        a->upper / b->upper
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

    int new_name = vector_length(out_state->env);
    Interval interval = (Interval) { .lower = value, .upper = value };
    vector_push(out_state->env, &interval);

    if (vector_push(out_state->stack, &new_name)) {
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

    int* name = vector_get(out_state->locals, load->index);
    if (!name) {
        LOG_ERROR("Name is not defined, locals len: %ld", vector_length(out_state->locals));
        return FAILURE;
    }

    if (vector_push(out_state->stack, name)) {
        return FAILURE;
    }

    return SUCCESS;
}

static int handle_store(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction)
        return FAILURE;

    StoreOP* store = &ir_instruction->data.store;
    if (!store)
        return FAILURE;

    int name_old;
    if (vector_pop(out_state->stack, &name_old)) {
        return FAILURE;
    }

    int new_name = vector_length(out_state->env);
    Interval iv = *(Interval*)vector_get(out_state->env, name_old);
    vector_push(out_state->env, &iv);

    if (store->index == vector_length(out_state->locals)) {
        vector_push(out_state->locals, &new_name);
    } else if (store->index < vector_length(out_state->locals)) {
        int* local_name = vector_get(out_state->locals, store->index);
        *local_name = new_name;
    } else {
        return FAILURE;
    }

    return SUCCESS;
}

static int handle_dup(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    int name;
    if (vector_pop(out_state->stack, &name)) {
        return FAILURE;
    }

    if (vector_push(out_state->stack, &name)) {
        return FAILURE;
    }
    if (vector_push(out_state->stack, &name)) {
        return FAILURE;
    }

    return SUCCESS;
}

static int handle_get(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    int name = vector_length(out_state->env);
    Interval interval = { .lower = 0, .upper = 1 };
    vector_push(out_state->env, &interval);

    if (vector_push(out_state->stack, &name)) {
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

    int name1, name2;
    if (vector_pop(out_state->stack, &name2)) return FAILURE;
    if (vector_pop(out_state->stack, &name1)) return FAILURE;

    Interval* interval1 = vector_get(out_state->env, name1);
    Interval* interval2 = vector_get(out_state->env, name2);

    Interval result;
    switch (binary->op) {
    case BO_MUL:
        result = interval_mul(interval1, interval2);
        break;

    case BO_ADD:
        result = interval_add(interval1, interval2);
        break;

    case BO_DIV:
        result = interval_div(interval1, interval2);
        break;

    case BO_SUB:
        result = interval_sub(interval1, interval2);
        break;

    default:
        LOG_ERROR("While abstract binary op");
        return FAILURE;
    }

    int new_name = vector_length(out_state->env);

    vector_push(out_state->env, &result);
    vector_push(out_state->stack, &new_name);

    return SUCCESS;
}

static int handle_new(IntervalState* st, IrInstruction* ins)
{
    (void)ins;
    int name = vector_length(st->env);
    Interval top = interval_top();
    vector_push(st->env, &top);
    vector_push(st->stack, &name);
    return SUCCESS;
}

static int handle_incr(IntervalState* st, IrInstruction* ins)
{
    if (!st || !ins) {
        return FAILURE;
    }

    IncrOP* incr = &ins->data.incr;
    int* iv_id   = vector_get(st->locals, incr->index);

    Interval iv = *(Interval*)vector_get(st->env, *iv_id);

    iv.lower++;

    if (iv.upper < INT_MAX)
        iv.upper++;

    *iv_id = vector_length(st->env);
    vector_push(st->env, &iv);

    return SUCCESS;
}

static int handle_negate(IntervalState* st, IrInstruction* ins)
{
    (void)ins;
    int new_name = vector_length(st->env);
    int iv_id;
    if (vector_pop(st->stack, &iv_id)) {
        return FAILURE;
    }

    Interval iv = *(Interval*)vector_get(st->env, iv_id);

    int tmp = iv.lower;
    iv.lower = -iv.upper;
    iv.upper = -tmp;

    vector_push(st->env, &iv);
    vector_push(st->stack, &new_name);
    return SUCCESS;
}

int interval_transfer(IntervalState* out_state, IrInstruction* ir_instruction)
{
    if (!out_state || !ir_instruction) {
        return FAILURE;
    }

    int result = SUCCESS;

    switch (ir_instruction->opcode) {
    case OP_LOAD:
        result = handle_load(out_state, ir_instruction);
        break;
    case OP_PUSH:
        result = handle_push(out_state, ir_instruction);
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
    case OP_GET:
        result = handle_get(out_state, ir_instruction);
        break;
    case OP_NEW:
        result = handle_new(out_state, ir_instruction);
        break;
    case OP_NEGATE:
        result = handle_negate(out_state, ir_instruction);
        break;
    case OP_INCR:
        result = handle_incr(out_state, ir_instruction);
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

static int handle_if_aux(IfCondition condition,
    Interval* x,
    Interval* y,
    Interval* true_branch,
    Interval* false_branch)
{
    int xL = x->lower;
    int xU = x->upper;
    int yL = y->lower;
    int yU = y->upper;

    Interval bottom = interval_bottom();

#define MAKE_INTERVAL_LOCAL(out, L, U) \
    do {                               \
        if ((L) <= (U)) {              \
            (out)->lower = (L);        \
            (out)->upper = (U);        \
        } else {                       \
            *(out) = bottom;           \
        }                              \
    } while (0)

    switch (condition) {
    case IF_EQ: {
        int L = (xL > yL ? xL : yL);
        int U = (xU < yU ? xU : yU);

        MAKE_INTERVAL_LOCAL(true_branch, L, U);

        if (L <= U) {
            *false_branch = *x;
        } else {
            *true_branch  = bottom;
            *false_branch = *x;
        }
        return SUCCESS;
    }

    case IF_NE: {
        int L = (xL > yL ? xL : yL);
        int U = (xU < yU ? xU : yU);

        MAKE_INTERVAL_LOCAL(false_branch, L, U);

        if (L <= U) {
            *true_branch = *x;
        } else {
            *true_branch  = *x;
            *false_branch = bottom;
        }
        return SUCCESS;
    }

    case IF_LT: {
        if (xU < yL) {
            *true_branch  = *x;
            *false_branch = bottom;
            return SUCCESS;
        }
        if (xL >= yU) {
            *true_branch  = bottom;
            *false_branch = *x;
            return SUCCESS;
        }
        if (yL > INT_MIN)
            yL--;

        MAKE_INTERVAL_LOCAL(true_branch, xL, yL);
        MAKE_INTERVAL_LOCAL(false_branch, yU, xU);
        return SUCCESS;
    }

    case IF_LE: {
        if (xU <= yL) {
            *true_branch  = *x;
            *false_branch = bottom;
            return SUCCESS;
        }
        if (xL > yU) {
            *true_branch  = bottom;
            *false_branch = *x;
            return SUCCESS;
        }

        MAKE_INTERVAL_LOCAL(true_branch,  xL,     yU);
        MAKE_INTERVAL_LOCAL(false_branch, yU + 1, xU);
        return SUCCESS;
    }

    case IF_GT: {
        if (xL > yU) {
            *true_branch  = *x;
            *false_branch = bottom;
            return SUCCESS;
        }
        if (xU <= yL) {
            *true_branch  = bottom;
            *false_branch = *x;
            return SUCCESS;
        }

        MAKE_INTERVAL_LOCAL(true_branch,
                            (xL > (yU + 1) ? xL : (yU + 1)),
                            xU);

        MAKE_INTERVAL_LOCAL(false_branch,
                            xL,
                            (xU < yU ? xU : yU));
        return SUCCESS;
    }

    case IF_GE: {
        if (xL >= yU) {
            *true_branch  = *x;
            *false_branch = bottom;
            return SUCCESS;
        }
        if (xU < yL) {
            *true_branch  = bottom;
            *false_branch = *x;
            return SUCCESS;
        }

        MAKE_INTERVAL_LOCAL(true_branch,
                            (xL > yL ? xL : yL),
                            xU);

        MAKE_INTERVAL_LOCAL(false_branch,
                            xL,
                            yL - 1);
        return SUCCESS;
    }

    default:
        return FAILURE;
    }
}

static int handle_if_zero(IntervalState* out_state_true, IntervalState* out_state_false, IrInstruction* ir_instruction)
{
    if (!out_state_true || !out_state_false || !ir_instruction) {
        return FAILURE;
    }

    IfOP* ift = &ir_instruction->data.ift;
    if (!ift) {
        return FAILURE;
    }

    int name, dummy;
    vector_pop(out_state_true->stack, &name);
    vector_pop(out_state_false->stack, &dummy);

    Interval* interval1 = vector_get(out_state_true->env, name);
    Interval interval2  = { .lower = 0, .upper = 0 };

    Interval true_branch, false_branch;
    handle_if_aux(ift->condition, interval1, &interval2, &true_branch, &false_branch);

    Interval* true_interval  = vector_get(out_state_true->env, name);
    *true_interval           = true_branch;

    Interval* false_interval = vector_get(out_state_false->env, name);
    *false_interval          = false_branch;

    return SUCCESS;
}

static int handle_if(IntervalState* out_state_true, IntervalState* out_state_false, IrInstruction* ir_instruction)
{
    if (!out_state_true || !out_state_false || !ir_instruction) {
        return FAILURE;
    }

    IfOP* ift = &ir_instruction->data.ift;
    if (!ift) {
        return FAILURE;
    }

    int name1, name2, dummy;
    vector_pop(out_state_true->stack, &name2);
    vector_pop(out_state_true->stack, &name1);

    vector_pop(out_state_false->stack, &dummy);
    vector_pop(out_state_false->stack, &dummy);

    Interval* interval1 = vector_get(out_state_true->env, name1);
    Interval* interval2 = vector_get(out_state_true->env, name2);

    Interval true_branch, false_branch;
    handle_if_aux(ift->condition, interval1, interval2, &true_branch, &false_branch);

    Interval* true_interval  = vector_get(out_state_true->env, name1);
    *true_interval           = true_branch;

    Interval* false_interval = vector_get(out_state_false->env, name1);
    *false_interval          = false_branch;

    return SUCCESS;
}

int interval_transfer_conditional(IntervalState* out_state_true, IntervalState* out_state_false, IrInstruction* ir_instruction)
{
    if (!out_state_true || !out_state_false || !ir_instruction) {
        return FAILURE;
    }

    int result = SUCCESS;

    switch (ir_instruction->opcode) {
    case OP_IF:
        result = handle_if(out_state_true, out_state_false, ir_instruction);
        break;
    case OP_IF_ZERO:
        result = handle_if_zero(out_state_true, out_state_false, ir_instruction);
        break;
    default:
        result = FAILURE;
        break;
    }

    if (result) {
        LOG_ERROR("%s", opcode_print(ir_instruction->opcode));
    }

    return SUCCESS;
}

int interval_transfer_invoke(IntervalState* out_state, IntervalState* in_state, int locals_num)
{
    if (!out_state || !in_state) {
        return FAILURE;
    }

    LOG_INFO("IN STATE ENV LEN: %ld, OUT STATE LEN %ld", vector_length(in_state->env), vector_length(out_state->env));
    LOG_INFO("LOCALS NUM: %d", locals_num);
    for (int i = locals_num - 1; i >= 0; i--) {
        interval_state_print(in_state);
        int id;
        vector_pop(in_state->stack, &id);

        LOG_INFO("1");
        Interval* iv = vector_get(in_state->env, id);
        LOG_INFO("2");

        int name = vector_length(out_state->env);
        LOG_INFO("3");

        if (!iv) {
            interval_state_print(in_state);
            LOG_INFO("AH");
        }

        Interval new_iv = { .lower = iv->lower, .upper = iv->upper };
        vector_push(out_state->locals, &name);
        LOG_INFO("4");
        vector_push(out_state->env, &new_iv);
        LOG_INFO("5");
    }

    return SUCCESS;
}

void interval_state_print(const IntervalState* st)
{
    if (!st || !st->env || !st->locals || !st->stack) {
        LOG_INFO("(null)");
        return;
    }

    if (is_interval_state_bottom(st)) {
        LOG_INFO("[⊥]");
        return;
    }

    LOG_INFO("Locals:");
    for (int i = 0; i < vector_length(st->locals); i++) {
        int name = *(int*)vector_get(st->locals, i);
        Interval* iv = vector_get(st->env, name);
        if (iv->lower == INT_MIN && iv->upper == INT_MAX)
            LOG_INFO("v%d = n%d [⊤]", i, name);
        else if (iv->lower == 1 && iv->upper == 0)
            LOG_INFO("v%d = n%d [⊥]", i, name);
        else
            LOG_INFO("v%d = n%d [%d,%d]", i, name, iv->lower, iv->upper);
    }

    LOG_INFO("Stack:");
    for (int i = 0; i < vector_length(st->stack); i++) {
        int* name = vector_get(st->stack, i);
        if (!name) {
            LOG_ERROR("NULL NAME ON STACK");
        }
        Interval* iv = vector_get(st->env, *name);
        if (!iv) {
            LOG_ERROR("NULL ENV INTERVAL");
        }
        if (iv->lower == INT_MIN && iv->upper == INT_MAX)
            LOG_INFO("[%d] = n%d [⊤]", i, *name);
        else if (iv->lower == 1 && iv->upper == 0)
            LOG_INFO("v%d = n%d [⊥]", i, *name);
        else
            LOG_INFO("[%d] = n%d [%d,%d]", i, *name, iv->lower, iv->upper);
    }

    LOG_INFO("Env (all names):");
    for (int i = 0; i < vector_length(st->env); i++) {
        Interval* iv = vector_get(st->env, i);
        if (iv->lower == INT_MIN && iv->upper == INT_MAX)
            LOG_INFO("n%d = [⊤]", i);
        else if (iv->lower == 1 && iv->upper == 0)
            LOG_INFO("v%d = [⊥]", i);
        else
            LOG_INFO("n%d = [%d,%d]", i, iv->lower, iv->upper);
    }
}

bool is_interval_state_bottom(const IntervalState* state)
{
    if (!state) {
        return true;
    }

    if (!vector_length(state->env) &&
        !vector_length(state->locals) &&
        !vector_length(state->stack)) {
        return true;
    }

    Interval bottom = interval_bottom();

    for (int i = 0; i < vector_length(state->env); i++) {
        Interval* iv = vector_get(state->env, i);
        if (bottom.lower == iv->lower && bottom.upper == iv->upper) {
            return true;
        }
    }

    return false;
}
