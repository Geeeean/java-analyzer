#ifndef DOMAIN_INTERVAL_H
#define DOMAIN_INTERVAL_H

#include "ir_instruction.h"
#include <vector.h>

typedef struct {
    int lower;
    int upper;
} Interval;

typedef struct {
    Vector* locals;
    Vector* stack;
} IntervalState;

IntervalState* interval_new_top_state(int num_vars);
int interval_state_copy(IntervalState* dst, const IntervalState* src);

int interval_join(IntervalState* acc, const IntervalState* new, int* changed);
int interval_intersection(IntervalState* acc, const IntervalState* constraint, int* changed);

int interval_widening(IntervalState* acc, const IntervalState* new, int* changed);
// int interval_narrowing(IntervalState* state_accumulator, IntervalState* state_prev, int* changed);

int interval_transfer(IntervalState* out_state, IrInstruction* ir_instruction);
int interval_transfer_conditional(IntervalState* out_state_true, IntervalState* out_state_false, IrInstruction* ir_instruction);

void interval_state_print(const IntervalState* st);
// int interval_transfer_assignment(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int* changed);
// int interval_transfer_mul(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);
// int interval_transfer_div(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);
// int interval_transfer_sum(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);
// int interval_transfer_sub(const IntervalState* in_state, IntervalState* out_state, int dst, int src1, int src2, int* changed);

#endif
