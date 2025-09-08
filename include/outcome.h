#ifndef OUTCOME_H
#define OUTCOME_H

#include <stdint.h>

typedef enum {
    OK,
    DIVIDE_BY_ZERO,
    ASSERTION_ERROR,
    OUT_OF_BOUNDS,
    NULL_POINTER,
    INFINITE_LOOP,
} outcome;

void print_outcome(const outcome oc, const uint8_t percentage);

#endif
