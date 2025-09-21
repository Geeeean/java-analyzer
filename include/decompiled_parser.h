#ifndef BYTECODE_PARSER_H
#define BYTECODE_PARSER_H

#include "config.h"
#include "method.h"

typedef enum {
    TYPE_INT,
    TYPE_BOOLEAN,
    TYPE_REFERENCE,
    TYPE_CLASS,
    TYPE_CHAR,
    TYPE_ARRAY,
    TYPE_VOID,
} ValueType;

typedef struct Value Value;
struct Value {
    ValueType type;
    union {
        int int_value;
        bool bool_value;
        char* char_value;
        void* ref_value;
        Value* array_value;
    } data;
};

typedef struct InstructionTable InstructionTable;

InstructionTable* instruction_table_build(Method* m, Config* cfg);
void instruction_table_delete(InstructionTable* instruction_table);
void value_print(const Value* value);

#endif
