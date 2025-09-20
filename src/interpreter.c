#include "interpreter.h"
#include "cJSON/cJSON.h"
#include <stdlib.h>
#include <string.h>

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

static const char* opcode_signature[] = {
    [OP_LOAD] = "load",
    [OP_PUSH] = "push",
    [OP_BINARY] = "binary",
    [OP_RETURN] = "return",
    [OP_GET] = "get",
    [OP_IF_ZERO] = "ifz",
    [OP_NEW] = "new",
    [OP_DUP] = "dup",
    [OP_INVOKE] = "invoke",
    [OP_THROW] = "throw",
};

typedef enum {
    VALUE_INT,
    VALUE_BOOLEAN,
    VALUE_REFERENCE,
    VALUE_CLASS,
    VALUE_STRING,
    VALUE_VOID
} ValueType;

// static const char* valuetype_signature[] = {
//     [VALUE_INT] = "integer",
// };

typedef enum {
    ADD,
    SUB,
    DIV,
    MUL,
} BinaryOperator;

typedef struct {
    ValueType type;
    union {
        int int_value;
        bool bool_value;
        void* ref_value;
        char* string_value;
    } data;
} Value;

typedef struct {
    Opcode opcode;
    int offset;

    union {
        struct {
            Value value;
        } push;

        struct {
            int index;
            ValueType value_type;
        } load;

        struct {
            ValueType type;
            BinaryOperator op;
        } binary;

        struct {
            ValueType type;
        } ret;
    } data;
} Instruction;

struct InstructionTable {
};

static cJSON* get_method(Method* m, cJSON* methods)
{
    cJSON* buffer;
    cJSON_ArrayForEach(buffer, methods)
    {
        cJSON* method_name_obj = cJSON_GetObjectItem(buffer, "name");
        if (cJSON_IsString(method_name_obj)) {
            char* method_name = cJSON_GetStringValue(method_name_obj);
            if (strcmp(method_name, method_get_name(m)) == 0) {
                return buffer;
            }
        }
    }

    return NULL;
}

static Opcode parse_offset(int* offset, cJSON* instruction_json)
{
    cJSON* offset_object = cJSON_GetObjectItem(instruction_json, "offset");
    if (!offset || !cJSON_IsNumber(offset_object)) {
        fprintf(stderr, "Offset is wrongly formatted or not exist\n");
        return 1;
    }

    *offset = (int)cJSON_GetNumberValue(offset_object);
    return 0;
}

static Opcode parse_opcode(Opcode* opcode, cJSON* instruction_json)
{
    cJSON* opr_object = cJSON_GetObjectItem(instruction_json, "opr");
    if (!opr_object || !cJSON_IsString(opr_object)) {
        fprintf(stderr, "Opr is wrongly formatted or not exist\n");
        return 1;
    }

    char* opr = cJSON_GetStringValue(opr_object);

    *opcode = -1;
    for (int i = 0; i < OP_COUNT; i++) {
        if (strcmp(opr, opcode_signature[i]) == 0) {
            *opcode = i;
        }
    }

    if (*opcode < 0) {
        fprintf(stderr, "Not known opr: %s\n", opr);
        return 2;
    }

    return 0;
}

static int parse_instruction(Instruction* instruction, cJSON* instruction_json)
{
    if (parse_offset(&instruction->offset, instruction_json)) {
        return 1;
    }

    if (parse_opcode(&instruction->opcode, instruction_json)) {
        return 2;
    }

    printf("OFFSET:%d, OPR: %s\n", instruction->offset, opcode_signature[instruction->opcode]);

    return 0;
}

static Instruction*
parse_bytecode(cJSON* method)
{
    cJSON* code = cJSON_GetObjectItem(method, "code");
    if (!code) {
        return NULL;
    }

    cJSON* bytecode = cJSON_GetObjectItem(code, "bytecode");
    if (!bytecode || !cJSON_IsArray(bytecode)) {
        return NULL;
    }

    cJSON* buffer;
    cJSON_ArrayForEach(buffer, bytecode)
    {
        Instruction instruction;
        parse_instruction(&instruction, buffer);

        // printf("Offset: %d\n", (int)cJSON_GetNumberValue(offset));
    }

    return NULL;
}

InstructionTable* build_instruction_table(Method* m, Config* cfg)
{
    InstructionTable* instr_table = NULL;

    char* source_decompiled = method_read(m, cfg, SRC_DECOMPILED);
    if (!source_decompiled) {
        goto cleanup;
    }

    cJSON* source_json = cJSON_Parse(source_decompiled);
    if (!source_json) {
        goto cleanup;
    }

    cJSON* methods = cJSON_GetObjectItem(source_json, "methods");
    if (!methods || !cJSON_IsArray(methods)) {
        goto cleanup;
    }

    cJSON* method = get_method(m, methods);
    if (!method) {
        goto cleanup;
    }

    parse_bytecode(method);

cleanup:
    cJSON_Delete(source_json);
    free(source_decompiled);

    return instr_table;
}
