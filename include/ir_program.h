#ifndef IR_PROGRAM
#define IR_PROGRAM

#include "config.h"
#include "ir_function.h"
#include "method.h"

IrFunction* ir_program_get_function(const Method* m, const Config* cfg);
void ir_program_free_all(void);

#endif
