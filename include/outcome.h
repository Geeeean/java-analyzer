#ifndef OUTCOME_H
#define OUTCOME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OC_OK,
    OC_DIVIDE_BY_ZERO,
    OC_ASSERTION_ERROR,
    OC_OUT_OF_BOUNDS,
    OC_NULL_POINTER,
    OC_INFINITE_LOOP,
} outcome;

typedef struct {
    int oc_ok;
    int oc_divide_by_zero;
    int oc_assertion_error;
    int oc_out_of_bounds;
    int oc_null_pointer;
    int oc_infinite_loop;
} Outcome;

Outcome new_outcome();
void print_outcome(Outcome oc);
void print_interpreter_outcome(const outcome oc);

#endif
