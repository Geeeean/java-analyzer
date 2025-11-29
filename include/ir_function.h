#ifndef IR_FUNCTION_H
#define IR_FUNCTION_H

#include "ir_instruction.h"

#define IR_FUNCTION_SIZE 4096

typedef struct IrFunction {
    int count;
    int capacity;
    IrInstruction* ir_instructions[IR_FUNCTION_SIZE];
} IrFunction;

IrFunction* ir_function_build(const Method* m, const Config* cfg);
void ir_function_delete(IrFunction* ir);

#endif
