#include "decompiled_parser.h"

#include "cJSON/cJSON.h"

#include <stdlib.h>
#include <string.h>

#define INSTRUCTION_TABLE_SIZE 100

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

static const char* load_type_signature[] = {
    [TYPE_INT] = "int",
    [TYPE_BOOLEAN] = "boolean",
    [TYPE_REFERENCE] = "ref",
};

static const char* push_type_signature[] = {
    [TYPE_INT] = "integer",
    [TYPE_BOOLEAN] = "boolean",
    [TYPE_ARRAY] = "string",
};

static const char* binary_type_signature[] = {
    [TYPE_INT] = "int",
};

static const char* return_type_signature[] = {
    [TYPE_INT] = "int",
    [TYPE_VOID] = "null",
};

typedef enum {
    ADD,
    SUB,
    DIV,
    MUL,
    BINARY_OPERATOR_COUNT
} BinaryOperator;

static const char* binary_operatore_signature[] = {
    [ADD] = "add",
    [SUB] = "sub",
    [DIV] = "div",
    [MUL] = "mul"
};

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
    union {
        PushOP push;
        LoadOP load;
        BinaryOP binary;
        ReturnOP ret;
    } data;
} Instruction;

struct InstructionTable {
    Instruction* instructions[INSTRUCTION_TABLE_SIZE];
    int count;
    int capacity;
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

static int parse_offset(int* offset, cJSON* instruction_json)
{
    cJSON* offset_object = cJSON_GetObjectItem(instruction_json, "offset");
    if (!offset || !cJSON_IsNumber(offset_object)) {
        fprintf(stderr, "Offset is wrongly formatted or not exist\n");
        return 1;
    }

    *offset = (int)cJSON_GetNumberValue(offset_object);
    return 0;
}

static int parse_opcode(Opcode* opcode, cJSON* instruction_json)
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

static int parse_load(LoadOP* load, cJSON* instruction_json)
{
    cJSON* index_obj = cJSON_GetObjectItem(instruction_json, "index");
    if (!index_obj || !cJSON_IsNumber(index_obj)) {
        fprintf(stderr, "Load instruction missing or invalid 'index' field\n");
        return 1;
    }
    load->index = (int)cJSON_GetNumberValue(index_obj);

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        fprintf(stderr, "Load instruction missing or invalid 'type' field\n");
        return 2;
    }

    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, load_type_signature[TYPE_INT]) == 0) {
        load->TYPE_type = TYPE_INT;
    } else if (strcmp(type, load_type_signature[TYPE_BOOLEAN]) == 0) {
        load->TYPE_type = TYPE_BOOLEAN;
    } else if (strcmp(type, load_type_signature[TYPE_REFERENCE]) == 0) {
        load->TYPE_type = TYPE_REFERENCE;
    } else {
        fprintf(stderr, "Unknown type in load instruction: %s\n", type);
        return 3;
    }

    return 0;
}

static int parse_push(PushOP* push, cJSON* instruction_json)
{
    cJSON* value = cJSON_GetObjectItem(instruction_json, "value");
    if (!value) {
        fprintf(stderr, "Load instruction missing or invalid 'value' field\n");
        return 1;
    }

    cJSON* type_obj = cJSON_GetObjectItem(value, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        fprintf(stderr, "Push instruction missing or invalid 'type' field\n");
        return 2;
    }
    char* type = cJSON_GetStringValue(type_obj);

    cJSON* inside_TYPE_obj = cJSON_GetObjectItem(value, "value");
    if (!inside_TYPE_obj) {
        fprintf(stderr, "Push instruction missing or invalid inside value, 'value' field\n");
        return 3;
    }
    char* inside_value = cJSON_GetStringValue(inside_TYPE_obj);

    if (strcmp(type, push_type_signature[TYPE_INT]) == 0) {
        push->value.type = TYPE_INT;
        if (!cJSON_IsNumber(inside_TYPE_obj)) {
            fprintf(stderr, "Push instruction invalid int number inside value, 'value' field\n");
            return 4;
        }
        push->value.data.int_value = cJSON_GetNumberValue(inside_TYPE_obj);
    }
    /*else if (strcmp(type, push_type_signature[TYPE_BOOLEAN]) == 0) {
        push->value.type = TYPE_BOOLEAN;
        sscanf(inside_value, "%d", &push->value.data.bo);
    }
    else if (strcmp(type, push_type_signature[TYPE_STRING]) == 0) {
        push->value.type = TYPE_STRING;
    }*/
    else {
        fprintf(stderr, "Unknown type in push instruction: %s\n", type);
        return 5;
    }

    return 0;
}

static int parse_binary(BinaryOP* binary, cJSON* instruction_json)
{
    cJSON* operant_obj = cJSON_GetObjectItem(instruction_json, "operant");
    if (!operant_obj || !cJSON_IsString(operant_obj)) {
        fprintf(stderr, "Binary instruction missing or invalid 'operant' field\n");
        return 1;
    }
    char* operant = cJSON_GetStringValue(operant_obj);

    binary->op = -1;
    for (int i = 0; i < BINARY_OPERATOR_COUNT; i++) {
        if (strcmp(operant, binary_operatore_signature[i]) == 0) {
            binary->op = i;
            break;
        }
    }

    if (binary->op < 0) {
        fprintf(stderr, "Binary instruction unknown operant: %s\n", operant);
        return 2;
    }

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        fprintf(stderr, "Binary instruction missing or invalid 'type' field\n");
        return 3;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(binary_type_signature[TYPE_INT], type) == 0) {
        binary->type = TYPE_INT;
    } else {
        fprintf(stderr, "Binary instruction unknown type: %s\n", type);
        return 3;
    }

    return 0;
}

static int parse_return(ReturnOP* ret, cJSON* instruction_json)
{
    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    // printf("%s\n",cJSON_GetStringValue(type_obj));
    if (!type_obj) {
        fprintf(stderr, "Return instruction missing or invalid 'type' field\n");
        return 1;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(return_type_signature[TYPE_INT], type) == 0) {
        ret->type = TYPE_INT;
    } else if (strcmp(return_type_signature[TYPE_VOID], type) == 0) {
        ret->type = TYPE_VOID;
    } else {
        fprintf(stderr, "Binary instruction unknown type: %s\n", type);
        return 2;
    }

    return 0;
}

static Instruction*
parse_instruction(cJSON* instruction_json, int* offset)
{
    Instruction* instruction = malloc(sizeof(Instruction));
    if (!instruction) {
        goto cleanup;
    }

    if (parse_offset(offset, instruction_json)) {
        goto cleanup;
    }

    if (parse_opcode(&instruction->opcode, instruction_json)) {
        goto cleanup;
    }

    switch (instruction->opcode) {
    case OP_LOAD:
        if (parse_load(&instruction->data.load, instruction_json)) {
            goto cleanup;
        }
        break;
    case OP_PUSH:
        if (parse_push(&instruction->data.push, instruction_json)) {
            goto cleanup;
        }
        break;
    case OP_GET:
        // todo
        break;
    case OP_BINARY:
        if (parse_binary(&instruction->data.binary, instruction_json)) {
            goto cleanup;
        }
        break;
    case OP_RETURN:
        if (parse_return(&instruction->data.ret, instruction_json)) {
            goto cleanup;
        }
        break;
        /*
            case OP_IF_ZERO:
                break;
            case OP_NEW:
                break;
            case OP_DUP:
                break;
            case OP_INVOKE:
                break;
            case OP_THROW:
                break;
            case OP_COUNT:
                break;
        */
    default:
        fprintf(stderr, "Unknown opcode: %d\n", instruction->opcode);
        goto cleanup;
    }

    return instruction;

cleanup:
    free(instruction);
    return NULL;
}

static InstructionTable*
parse_bytecode(cJSON* method)
{
    InstructionTable* instruction_table = calloc(1, sizeof(InstructionTable));
    if (!instruction_table) {
        goto cleanup;
    }

    instruction_table->capacity = INSTRUCTION_TABLE_SIZE;
    instruction_table->count = 0;

    cJSON* code = cJSON_GetObjectItem(method, "code");
    if (!code) {
        goto cleanup;
    }

    cJSON* bytecode = cJSON_GetObjectItem(code, "bytecode");
    if (!bytecode || !cJSON_IsArray(bytecode)) {
        goto cleanup;
    }

    cJSON* buffer;
    cJSON_ArrayForEach(buffer, bytecode)
    {
        // char* json_string_formatted = cJSON_Print(buffer);
        // if (json_string_formatted) {
        //     printf("%s\n\n", json_string_formatted);
        //     free(json_string_formatted);
        // }

        int offset;
        Instruction* instruction = parse_instruction(buffer, &offset);
        if (!instruction) {
            goto cleanup;
        }
        instruction_table->instructions[offset] = instruction;
    }

    return instruction_table;

cleanup:
    instruction_table_delete(instruction_table);
    return NULL;
}

InstructionTable* instruction_table_build(Method* m, Config* cfg)
{
    if (!m || !cfg) {
        return NULL;
    }

    char* source_decompiled = NULL;
    cJSON* source_json = NULL;
    InstructionTable* instruction_table = NULL;

    source_decompiled = method_read(m, cfg, SRC_DECOMPILED);
    if (!source_decompiled) {
        goto cleanup;
    }

    source_json = cJSON_Parse(source_decompiled);
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

    instruction_table = parse_bytecode(method);

cleanup:
    if (source_json) {
        cJSON_Delete(source_json);
    }
    free(source_decompiled);

    return instruction_table;
}

void instruction_table_delete(InstructionTable* instruction_table)
{

    if (instruction_table) {
        for (int i = 0; i < INSTRUCTION_TABLE_SIZE; i++) {
            free(instruction_table->instructions[i]);
        }
    }

    free(instruction_table);
}

void value_print(const Value* value)
{
    if (value == NULL) {
        printf("NULL");
        return;
    }

    switch (value->type) {
    case TYPE_INT:
        printf("%d", value->data.int_value);
        break;
    case TYPE_BOOLEAN:
        printf("%s", value->data.bool_value ? "true" : "false");
        break;
    case TYPE_ARRAY:
        printf("[");
        if (value->data.array_value != NULL) {
            Value* arr = value->data.array_value;
            for (int i = 0; arr[i].type != TYPE_VOID; i++) {
                if (i > 0)
                    printf(", ");
                value_print(&arr[i]);
            }
        }
        printf("]");
        break;
    case TYPE_VOID:
        printf("void");
        break;
    default:
        printf("unknown_value");
        break;
    }
}
