#include "domain_interval.h"
#include "common.h"
#include <limits.h>

#define MUL_COMB 4

int interval_join(IntervalState* state_accumulator, IntervalState* state_new, int* changed)
{
    if (!state_accumulator || !state_new || !changed) {
        return FAILURE;
    }

    *changed = 0;

    Vector* states1 = state_accumulator->vars;
    Vector* states2 = state_new->vars;

    if (!states1 || !states2 || vector_length(states1) != vector_length(states2)) {
        return FAILURE;
    }

    for (int i = 0; i < vector_length(states1); i++) {
        Interval* int1 = (Interval*)vector_get(states1, i);
        Interval* int2 = (Interval*)vector_get(states2, i);

        if (!int1 || !int2) {
            *changed = 0;
            return FAILURE;
        }

        int lower = MIN(int1->lower, int2->lower);
        int upper = MAX(int1->upper, int2->upper);

        if (int1->lower != lower || int1->upper != upper) {
            int1->lower = lower;
            int1->upper = upper;
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_intersection(IntervalState* state_accumulator, IntervalState* state_constraint, int* changed)
{
    if (!state_accumulator || !state_constraint || !changed) {
        return FAILURE;
    }

    *changed = 0;

    Vector* states1 = state_accumulator->vars;
    Vector* states2 = state_constraint->vars;

    if (!states1 || !states2 || vector_length(states1) != vector_length(states2)) {
        return FAILURE;
    }

    for (int i = 0; i < vector_length(states1); i++) {
        Interval* int1 = (Interval*)vector_get(states1, i);
        Interval* int2 = (Interval*)vector_get(states2, i);

        if (!int1 || !int2) {
            *changed = 0;
            return FAILURE;
        }

        int lower = MAX(int1->lower, int2->lower);
        int upper = MIN(int1->upper, int2->upper);

        if (int1->lower != lower || int1->upper != upper) {
            int1->lower = lower;
            int1->upper = upper;
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_widening(IntervalState* state_accumulator, IntervalState* state_new, int* changed)
{
    if (!state_accumulator || !state_new || !changed) {
        return FAILURE;
    }

    *changed = 0;

    Vector* states1 = state_accumulator->vars;
    Vector* states2 = state_new->vars;

    if (!states1 || !states2 || vector_length(states1) != vector_length(states2)) {
        return FAILURE;
    }

    for (int i = 0; i < vector_length(states1); i++) {
        Interval* int1 = (Interval*)vector_get(states1, i);
        Interval* int2 = (Interval*)vector_get(states2, i);

        if (!int1 || !int2) {
            *changed = 0;
            return FAILURE;
        }

        if (int1->lower > int2->lower) {
            int1->lower = INT_MIN;
            *changed = 1;
        }

        if (int1->upper < int2->upper) {
            int1->upper = INT_MAX;
            *changed = 1;
        }
    }

    return SUCCESS;
}

int interval_narrowing(IntervalState* state_accumulator, IntervalState* state_prev, int* changed)
{
    return interval_intersection(state_accumulator, state_prev, changed);
}

int interval_transfer_assignment(const IntervalState* in_state, IntervalState* out_state, int dst, int src, int* changed)
{
    if (!in_state || !out_state || !changed) {
        return FAILURE;
    }

    *changed = 0;
    Vector* states1 = in_state->vars;
    Vector* states2 = out_state->vars;

    if (!states1 || !states2) {
        return FAILURE;
    }

    if (vector_copy(states2, states1)) {
        return FAILURE;
    }

    Interval* interval_out = vector_get(states2, dst);
    Interval* interval_in = vector_get(states1, src);

    if (!interval_out || !interval_in) {
        return FAILURE;
    }

    if (interval_out->lower != interval_in->lower || interval_out->upper != interval_in->upper) {
        *changed = 1;
        interval_out->lower = interval_in->lower;
        interval_out->upper = interval_in->upper;
    }

    return SUCCESS;
}

int interval_transfer_mul(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed)
{
    if (!in_state || !out_state || !changed) {
        return FAILURE;
    }

    *changed = 0;
    Vector* states1 = in_state->vars;
    Vector* states2 = out_state->vars;

    if (!states1 || !states2) {
        return FAILURE;
    }

    if (vector_copy(states2, states1)) {
        return FAILURE;
    }

    Interval* interval_out = vector_get(states2, dst);
    Interval* interval_in1 = vector_get(states1, src1);
    Interval* interval_in2 = vector_get(states1, src2);

    if (!interval_out || !interval_in1 || !interval_in2) {
        return FAILURE;
    }

    int min = INT_MAX;
    int max = INT_MIN;

    int products[MUL_COMB] = {
        [0] = interval_in1->lower * interval_in2->lower,
        [1] = interval_in1->lower * interval_in2->upper,
        [2] = interval_in1->upper * interval_in2->lower,
        [3] = interval_in1->upper * interval_in2->upper,
    };

    for (int i = 0; i < MUL_COMB; i++) {
        if (min > products[i]) {
            min = products[i];
        }

        if (max < products[i]) {
            max = products[i];
        }
    }

    if (interval_out->lower != min || interval_out->upper != max) {
        *changed = 1;
        interval_out->lower = min;
        interval_out->upper = max;
    }

    return SUCCESS;
}

int interval_transfer_sum(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed)
{
    if (!in_state || !out_state || !changed) {
        return FAILURE;
    }

    *changed = 0;
    Vector* states1 = in_state->vars;
    Vector* states2 = out_state->vars;

    if (!states1 || !states2) {
        return FAILURE;
    }

    if (vector_copy(states2, states1)) {
        return FAILURE;
    }

    Interval* interval_out = vector_get(states2, dst);
    Interval* interval_in1 = vector_get(states1, src1);
    Interval* interval_in2 = vector_get(states1, src2);

    if (!interval_out || !interval_in1 || !interval_in2) {
        return FAILURE;
    }

    int upper_sum = interval_in1->upper + interval_in2->upper;
    int lower_sum = interval_in1->lower + interval_in2->lower;

    if (interval_out->lower != lower_sum || interval_out->upper != upper_sum) {
        *changed = 1;
        interval_out->lower = lower_sum;
        interval_out->upper = upper_sum;
    }

    return SUCCESS;
}

int interval_transfer_div(const IntervalState* in_state, IntervalState* out_state,
    int dst, int src1, int src2, int* changed)
{
    if (!in_state || !out_state || !changed) {
        return FAILURE;
    }

    *changed = 0;

    Vector* states1 = in_state->vars;
    Vector* states2 = out_state->vars;

    if (!states1 || !states2 || vector_length(states1) != vector_length(states2)) {
        return FAILURE;
    }

    // Copy entire state
    for (int i = 0; i < vector_length(states1); i++) {
        Interval* in_i = (Interval*)vector_get(states1, i);
        Interval* out_i = (Interval*)vector_get(states2, i);
        if (!in_i || !out_i) {
            return FAILURE;
        }
        out_i->lower = in_i->lower;
        out_i->upper = in_i->upper;
    }

    Interval* interval_out = vector_get(states2, dst);
    Interval* interval_in1 = vector_get(states1, src1);
    Interval* interval_in2 = vector_get(states1, src2);

    if (!interval_out || !interval_in1 || !interval_in2) {
        return FAILURE;
    }

    // Check if denominator interval crosses zero
    if (interval_in2->lower <= 0 && interval_in2->upper >= 0) {
        // Division by zero possible → return TOP
        int lower = INT_MIN;
        int upper = INT_MAX;

        if (interval_out->lower != lower || interval_out->upper != upper) {
            interval_out->lower = lower;
            interval_out->upper = upper;
            *changed = 1;
        }

        return SUCCESS;
    }

    // Safe division (no zero inside interval)
    int a = interval_in1->lower;
    int b = interval_in1->upper;
    int c = interval_in2->lower;
    int d = interval_in2->upper;

    int candidates[4] = { a / c, a / d, b / c, b / d };

    int min = candidates[0];
    int max = candidates[0];
    for (int i = 1; i < 4; i++) {
        if (candidates[i] < min)
            min = candidates[i];
        if (candidates[i] > max)
            max = candidates[i];
    }

    if (interval_out->lower != min || interval_out->upper != max) {
        interval_out->lower = min;
        interval_out->upper = max;
        *changed = 1;
    }

    return SUCCESS;
}

int interval_transfer_sub(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed)
{
    if (!in_state || !out_state || !changed) {
        return FAILURE;
    }

    *changed = 0;
    Vector* states1 = in_state->vars;
    Vector* states2 = out_state->vars;

    if (!states1 || !states2) {
        return FAILURE;
    }

    if (vector_copy(states2, states1)) {
        return FAILURE;
    }

    Interval* interval_out = vector_get(states2, dst);
    Interval* interval_in1 = vector_get(states1, src1);
    Interval* interval_in2 = vector_get(states1, src2);

    if (!interval_out || !interval_in1 || !interval_in2) {
        return FAILURE;
    }

    int upper_sub = interval_in1->upper - interval_in2->lower;
    int lower_sub = interval_in1->lower - interval_in2->upper;

    if (interval_out->lower != lower_sub || interval_out->upper != upper_sub) {
        *changed = 1;
        interval_out->lower = lower_sub;
        interval_out->upper = upper_sub;
    }

    return SUCCESS;
}
