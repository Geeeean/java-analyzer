#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "decompiled_parser.h"
#include "method.h"

typedef enum {
    RT_OK,
    RT_DIVIDE_BY_ZERO,
    RT_ASSERTION_ERR,
    RT_INFINITE,
    RT_CANT_BUILD_FRAME,
    RT_NULL_PARAMETERS,
    RT_UNKNOWN_ERROR,
} RuntimeResult;

RuntimeResult interpreter_run(InstructionTable* instruction_table, const Method* m, const char* parameters);

#endif
