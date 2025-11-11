#ifndef IR_PROGRAM
#define IR_PROGRAM

#include "cfg.h"
#include "config.h"
#include "ir_function.h"
#include "method.h"

IrFunction* ir_program_get_function_ir(const Method* m, const Config* cfg);
Cfg* ir_program_get_cfg(const Method* m, const Config* cfg);

#endif
