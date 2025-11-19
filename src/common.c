#include "common.h"
#include <limits.h>

int min(Vector* v)
{
    int res = INT_MAX;
    for (int i = 0; i < vector_length(v); i++) {
        int* value = vector_get(v, i);
        if (*value < res) {
            res = *value;
        }
    }

    return res;
}
