#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "config.h"
#include "method.h"

typedef struct InstructionTable InstructionTable;

InstructionTable* build_instruction_table(Method* m, Config* cfg);

#endif
