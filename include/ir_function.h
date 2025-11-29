#ifndef IR_FUNCTION_H
#define IR_FUNCTION_H

#include "config.h"
#include "method.h"

typedef struct {
    Vector* ir_instructions;
} IrFunction;

IrFunction* ir_function_build(const Method* m, const Config* cfg);
void ir_function_delete(IrFunction* ir_function);

#endif
