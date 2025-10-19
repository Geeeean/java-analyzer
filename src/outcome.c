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

static void print_single_outcome(const outcome oc, const uint8_t percentage)
{
    printf("%s;%d%%\n", outcome_strings[oc], percentage);
}

Outcome new_outcome()
{
    return (Outcome) {
        .oc_assertion_error = 50,
        .oc_divide_by_zero = 50,
        .oc_infinite_loop = 50,
        .oc_null_pointer = 50,
        .oc_ok = 50,
        .oc_out_of_bounds = 50,
    };
}

void print_outcome(Outcome oc)
{
    print_single_outcome(OC_ASSERTION_ERROR, oc.oc_assertion_error);
    print_single_outcome(OC_DIVIDE_BY_ZERO, oc.oc_divide_by_zero);
    print_single_outcome(OC_INFINITE_LOOP, oc.oc_infinite_loop);
    print_single_outcome(OC_NULL_POINTER, oc.oc_null_pointer);
    print_single_outcome(OC_OK, oc.oc_ok);
    print_single_outcome(OC_OUT_OF_BOUNDS, oc.oc_out_of_bounds);
}

void print_interpreter_outcome(const outcome oc)
{
    printf("%s\n", outcome_strings[oc]);
}
