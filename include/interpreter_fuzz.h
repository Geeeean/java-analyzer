#ifndef INTERPRETER_FUZZ_H
#define INTERPRETER_FUZZ_H

#include <stdint.h>
#include <stddef.h>

#include "config.h"
#include "method.h"
#include "value.h"
#include "heap.h"
#include "cli.h"
#include "runtime_result.h"

typedef struct VMContext VMContext;

VMContext* fuzz_interpreter_setup(const Method* m,
                                  const Options* opts,
                                  const Config* cfg,
                                  uint8_t* thread_bitmap);

RuntimeResult fuzz_interpreter_run(VMContext* vm);

void fuzz_VMContext_reset(VMContext* vm);

void fuzz_VMContext_set_locals(VMContext* vm,
                          Value* new_locals,
                          int count);

void fuzz_VMContext_set_coverage_bitmap(VMContext* vm,
                                   uint8_t* bitmap);

Heap* fuzz_VMContext_get_heap(VMContext* vm);

void fuzz_interpreter_free(VMContext* vm);

Value* fuzz_build_locals_fast(Heap* heap,
                         const Method* m,
                         const uint8_t* data,
                         size_t len,
                         int* locals_count);

#endif /* INTERPRETER_FUZZ_H */
