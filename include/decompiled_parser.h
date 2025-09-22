#ifndef BYTECODE_PARSER_H
#define BYTECODE_PARSER_H

#include "config.h"
#include "method.h"

#define INSTRUCTION_TABLE_SIZE 100

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

Value value_deep_copy(const Value* src);

typedef enum {
    OP_LOAD,
    OP_PUSH,
    OP_BINARY,
    OP_GET,
    OP_RETURN,
    OP_IF_ZERO,
    OP_NEW,
    OP_DUP,
    OP_INVOKE,
    OP_THROW,
    OP_COUNT
} Opcode;

typedef enum {
    ADD,
    SUB,
    DIV,
    MUL,
    BINARY_OPERATOR_COUNT
} BinaryOperator;

typedef struct {
    Value value;
} PushOP;

typedef struct {
    int index;
    ValueType TYPE_type;
} LoadOP;

typedef struct {
    ValueType type;
    BinaryOperator op;
} BinaryOP;

typedef struct {
    ValueType type;
} ReturnOP;

typedef struct {
    Opcode opcode;
    int seq;
    union {
        PushOP push;
        LoadOP load;
        BinaryOP binary;
        ReturnOP ret;
    } data;
} Instruction;

typedef struct {
    Instruction* instructions[INSTRUCTION_TABLE_SIZE];
    int count;
    int capacity;
} InstructionTable;

InstructionTable* instruction_table_build(Method* m, Config* cfg);
void instruction_table_delete(InstructionTable* instruction_table);
void value_print(const Value* value);

int value_add(Value* value1, Value* value2, Value* result);
int value_mul(Value* value1, Value* value2, Value* result);
int value_sub(Value* value1, Value* value2, Value* result);
int value_div(Value* value1, Value* value2, Value* result);
#endif
