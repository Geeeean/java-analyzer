#ifndef OUTCOME_H
#define OUTCOME_H

#include <stdint.h>

typedef enum {
    OC_OK,
    OC_DIVIDE_BY_ZERO,
    OC_ASSERTION_ERROR,
    OC_OUT_OF_BOUNDS,
    OC_NULL_POINTER,
    OC_INFINITE_LOOP,
} outcome;

void print_outcome(const outcome oc, const uint8_t percentage);
void print_interpreter_outcome(const outcome oc);

#endif
