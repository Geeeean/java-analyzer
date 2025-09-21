#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "decompiled_parser.h"
#include "method.h"

int interpreter_execute(InstructionTable* instruction_table, const Method* m, char* parameters);

#endif
