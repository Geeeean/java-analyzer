#ifndef IR_FUNCTION_H
#define IR_FUNCTION_H

#include "config.h"
#include "ir_instruction.h"
#include "method.h"

// todo make dynamic
#define IR_FUNCTION_SIZE 100

typedef struct {
    IrInstruction* ir_instructions[IR_FUNCTION_SIZE];
    int count;
    int capacity;
} IrFunction;

IrFunction* ir_function_build(const Method* m, const Config* cfg);
void ir_function_delete(IrFunction* ir_function);

#endif
