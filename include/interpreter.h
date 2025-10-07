#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "cli.h"
#include "method.h"
#include "opcode.h"

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

typedef struct CallStack CallStack;

// RuntimeResult interpreter_run(InstructionTable* instruction_table, const Method* m, const char* parameters);
CallStack* interpreter_setup(const Method* m, const Options* opts, const Config* cfg);
RuntimeResult interpreter_run(CallStack* call_stack, const Config* cfg);

#endif
