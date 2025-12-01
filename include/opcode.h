#ifndef BYTECODE_PARSER_H
#define BYTECODE_PARSER_H

#include "cJSON/cJSON.h"
#include "method.h"
#include "type.h"
#include "value.h"

// todo: make dynamic
#define INSTRUCTION_TABLE_SIZE 100

extern char args_type_signature[];

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
    OP_STORE,
    OP_GOTO,
    OP_CAST,
    OP_NEW_ARRAY,
    OP_ARRAY_LOAD,
    OP_ARRAY_STORE,
    OP_ARRAY_LENGTH,
    OP_INCR,
    OP_NEGATE,
    OP_COMPARE_FLOATING,
    OP_COUNT
} Opcode;

typedef enum {
    BO_ADD,
    BO_SUB,
    BO_DIV,
    BO_MUL,
    BO_REM,
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
    Type* type;
} LoadOP;

typedef struct {
    Type* type;
    BinaryOperator op;
} BinaryOP;

typedef struct {
    Type* type;
} ReturnOP;

typedef struct {
} GetOP;

typedef struct {
    char* method_name;
    char* ref_name;
    int args_len;
    Type** args; // must be freed
    Type* return_type;
} InvokeOP;

typedef struct {
} ThrowOP;

typedef struct {
    IfCondition condition;
    int target;
} IfOP;

typedef struct {
    Type* type;
    int index;
} StoreOP;

typedef struct {
    int target;
} GotoOP;

typedef struct {
    Type* from;
    Type* to;
} CastOP;

typedef struct {
    int words;
} DupOP;

typedef struct {
    int dim;
    Type* type;
} NewArrayOP;

typedef struct {
    Type* type;
} ArrayStoreOP;

typedef struct {
    Type* type;
} ArrayLoadOP;

typedef struct {
} ArrayLengthOP;

typedef struct {
    int index;
    int amount;
} IncrOP;

typedef struct {
    Type* type;
} NegateOP;

typedef struct {
    Type* type;
    int onnan;
} CompareFloatingOP;

typedef struct {
    Opcode opcode;
    int seq;
    union {
        PushOP push;
        LoadOP load;
        BinaryOP binary;
        ReturnOP ret;
        GetOP get;
        InvokeOP invoke;
        IfOP ift;
        ThrowOP trw;
        StoreOP store;
        GotoOP go2;
        CastOP cast;
        DupOP dup;
        NewArrayOP new_array;
        ArrayStoreOP array_store;
        ArrayLoadOP array_load;
        ArrayLengthOP array_length;
        IncrOP incr;
    } data;
} Instruction;

typedef struct {
    Instruction* instructions[INSTRUCTION_TABLE_SIZE];
    int count;
    int capacity;
} InstructionTable;

InstructionTable* instruction_table_build(const Method* m, const Config* cfg);
// void value_print(const Value* value);

const char* opcode_print(Opcode opcode);
Opcode opcode_parse(cJSON* instruction_json);

cJSON* opcode_get_method(Method* m, cJSON* methods);

#endif
