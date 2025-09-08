#include "outcome.h"

#include <stdio.h>

#define OK_TAG "ok"
#define DIVIDE_BY_ZERO_TAG "divide by zero"
#define ASSERTION_ERROR_TAG "assertion error"
#define OUT_OF_BOUNDS_TAG "out of bounds"
#define NULL_POINTER_TAG "null pointer"
#define INFINITE_LOOP_TAG "*"

static const char* outcome_strings[] = {
    [OK] = OK_TAG,
    [DIVIDE_BY_ZERO] = DIVIDE_BY_ZERO_TAG,
    [ASSERTION_ERROR] = ASSERTION_ERROR_TAG,
    [OUT_OF_BOUNDS] = OUT_OF_BOUNDS_TAG,
    [NULL_POINTER] = NULL_POINTER_TAG,
    [INFINITE_LOOP] = INFINITE_LOOP_TAG,

};

void print_outcome(const outcome oc, const uint8_t percentage)
{
    printf("%s;%d%%\n", outcome_strings[oc], percentage);
}
