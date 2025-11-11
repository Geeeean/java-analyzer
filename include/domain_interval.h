#ifndef DOMAIN_INTERVAL_H
#define DOMAIN_INTERVAL_H

#include <vector.h>

typedef struct {
    int lower;
    int upper;
} Interval;

typedef struct {
    Vector* vars; // vector of intervals
} IntervalState;

int interval_join(IntervalState* state_accumulator, IntervalState* state_new, int* changed);
int interval_intersection(IntervalState* state_accumulator, IntervalState* state_constraint, int* changed);

int interval_widening(IntervalState* state_accumulator, IntervalState* state_new, int* changed);
int interval_narrowing(IntervalState* state_accumulator, IntervalState* state_prev, int* changed);

int interval_transfer_assignment(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int* changed);
int interval_transfer_mul(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);
int interval_transfer_div(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);
int interval_transfer_sum(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);
int interval_transfer_sub(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);

#endif
