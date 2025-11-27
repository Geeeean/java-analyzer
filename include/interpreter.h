#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "cli.h"
#include "method.h"
#include "value.h"

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

VMContext* persistent_interpreter_setup(const Method* m,
                                        const Options* opts,
                                        const Config* cfg,
                                        uint8_t* thread_bitmap);

void VMContext_reset(VMContext* vm);

RuntimeResult interpreter_run(VMContext* vm_context);

size_t interpreter_instruction_count(const Method* m, const Config* cfg);
void interpreter_free(VMContext* vm);
void instruction_table_map_free();
void VMContext_set_coverage_bitmap(VMContext* vm, uint8_t* bitmap);
void VMContext_set_locals(VMContext* vm, Value* locals, int locals_count);
Value* build_locals_from_str(const Method* m, char* parameters, int* locals_count);
Value* build_locals_fast(const Method* m,
                         const uint8_t* data,
                         size_t len,
                         int* locals_count);

#endif
