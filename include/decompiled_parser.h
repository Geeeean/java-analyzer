#ifndef BYTECODE_PARSER_H
#define BYTECODE_PARSER_H

#include "config.h"
#include "method.h"

#define INSTRUCTION_TABLE_SIZE 100

typedef enum {
    BO_OK,
    BO_DIFFERENT_TYPES,
    BO_NOT_SUPPORTED_TYPES,
    BO_DIVIDE_BY_ZERO,
} BinaryResult;

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
        char char_value;
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
    OP_IF,
    OP_NEW,
    OP_DUP,
    OP_INVOKE,
    OP_THROW,
    OP_COUNT
} Opcode;

typedef enum {
    BO_ADD,
    BO_SUB,
    BO_DIV,
    BO_MUL,
    BO_COUNT
} BinaryOperator;

typedef enum {
    IF_EQ,
    IF_NE,
    IF_GT,
    IF_LT,
    IF_GE,
    IF_LE,
    IF_CONDITION_COUNT
} IfCondition;

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
} GetOP;

typedef struct {
} ThrowOP;

typedef struct {
    IfCondition condition;
    int target;
} IfOP;

typedef struct {
    Opcode opcode;
    int seq;
    union {
        PushOP push;
        LoadOP load;
        BinaryOP binary;
        ReturnOP ret;
        GetOP get;
        IfOP ift;
        ThrowOP trw;
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

BinaryResult value_add(Value* value1, Value* value2, Value* result);
BinaryResult value_mul(Value* value1, Value* value2, Value* result);
BinaryResult value_sub(Value* value1, Value* value2, Value* result);
BinaryResult value_div(Value* value1, Value* value2, Value* result);

const char* opcode_print(Opcode opcode);

#endif
