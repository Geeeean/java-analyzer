#include "outcome.h"

#include <stdio.h>

#define OK_TAG "ok"
#define DIVIDE_BY_ZERO_TAG "divide by zero"
#define ASSERTION_ERROR_TAG "assertion error"
#define OUT_OF_BOUNDS_TAG "out of bounds"
#define NULL_POINTER_TAG "null pointer"
#define INFINITE_LOOP_TAG "*"

static const char* outcome_strings[] = {
    [OC_OK] = OK_TAG,
    [OC_DIVIDE_BY_ZERO] = DIVIDE_BY_ZERO_TAG,
    [OC_ASSERTION_ERROR] = ASSERTION_ERROR_TAG,
    [OC_OUT_OF_BOUNDS] = OUT_OF_BOUNDS_TAG,
    [OC_NULL_POINTER] = NULL_POINTER_TAG,
    [OC_INFINITE_LOOP] = INFINITE_LOOP_TAG,
};

void print_outcome(const outcome oc, const uint8_t percentage)
{
    printf("%s;%d%%\n", outcome_strings[oc], percentage);
}

void print_interpreter_outcome(const outcome oc)
{
    printf("%s\n", outcome_strings[oc]);
}
