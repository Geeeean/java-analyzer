#ifndef RUNTIME_RESULT_H
#define RUNTIME_RESULT_H

typedef enum {
    RT_OK = 0,
    RT_DIVIDE_BY_ZERO,
    RT_ASSERTION_ERR,
    RT_OUT_OF_BOUNDS,
    RT_NULL_POINTER,
    RT_UNKNOWN_ERROR,
    RT_INFINITE
} RuntimeResult;

#endif