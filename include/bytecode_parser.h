#ifndef BYTECODE_PARSER_H
#define BYTECODE_PARSER_H

#include "config.h"
#include "method.h"

typedef struct InstructionTable InstructionTable;

InstructionTable* instruction_table_build(Method* m, Config* cfg);
void instruction_table_delete(InstructionTable* instruction_table);

#endif
