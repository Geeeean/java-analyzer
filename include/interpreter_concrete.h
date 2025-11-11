#ifndef INTERPRETER_CONCRETE_H
#define INTERPRETER_CONCRETE_H

#include "cli.h"
#include "method.h"

typedef enum {
    RT_OK,
    RT_DIVIDE_BY_ZERO,
    RT_ASSERTION_ERR,
    RT_INFINITE,
    RT_OUT_OF_BOUNDS,
    RT_NULL_POINTER,
    RT_CANT_BUILD_FRAME,
    RT_NULL_PARAMETERS,
    RT_UNKNOWN_ERROR,
} RuntimeResult;

typedef struct VMContext VMContext;

VMContext* interpreter_concrete_setup(const Method* m, const Options* opts, const Config* cfg);
RuntimeResult interpreter_concrete_run(VMContext* vm_context);

#endif
