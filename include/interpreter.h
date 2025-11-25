#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "cli.h"
#include "method.h"
#include <stdint.h>

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

VMContext* interpreter_setup(const Method* m, const Options* opts, const Config* cfg, uint8_t* thread_bitmap);
RuntimeResult interpreter_run(VMContext* vm_context);

size_t interpreter_instruction_count(const Method* m, const Config* cfg);
void interpreter_free(VMContext* vm);
void instruction_table_map_free();

#endif
