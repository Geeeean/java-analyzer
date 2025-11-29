#ifndef DOMAIN_INTERVAL_H
#define DOMAIN_INTERVAL_H

#include "ir_instruction.h"

#include <stdbool.h>
#include <vector.h>

typedef struct {
    int lower;
    int upper;
} Interval;

typedef struct {
    Vector* locals; // Vector<int>
    Vector* stack; // Vector<int>
    Vector* env; // Vector<Interval>
} IntervalState;

IntervalState* interval_new_top_state(int num_vars);
IntervalState* interval_new_bottom_state(int num_locals);
int interval_state_copy(IntervalState* dst, const IntervalState* src);
bool is_interval_state_bottom(const IntervalState* state);

int interval_join(IntervalState* acc, const IntervalState* new, int* changed);
int interval_intersection(IntervalState* acc, const IntervalState* constraint, int* changed);

int interval_widening(IntervalState* acc, const IntervalState* new, int* changed);

int interval_transfer(IntervalState* out_state, IrInstruction* ir_instruction);
int interval_transfer_invoke(IntervalState* out_state, IntervalState* in_state, int locals_num);
int interval_transfer_conditional(IntervalState* out_state_true, IntervalState* out_state_false, IrInstruction* ir_instruction);

void interval_state_print(const IntervalState* st);
void interval_state_delete(IntervalState* st);

#endif
