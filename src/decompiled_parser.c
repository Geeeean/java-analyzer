#include "decompiled_parser.h"

#include "cJSON/cJSON.h"

#include <stdlib.h>
#include <string.h>

static const char* opcode_signature[] = {
    [OP_LOAD] = "load",
    [OP_PUSH] = "push",
    [OP_BINARY] = "binary",
    [OP_GET] = "get",
    [OP_RETURN] = "return",
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

static const char* binary_operatore_signature[] = {
    [BO_ADD] = "add",
    [BO_SUB] = "sub",
    [BO_DIV] = "div",
    [BO_MUL] = "mul"
};

static const char* condition_signature[] = {
    [IFZ_EQ] = "eq",
    [IFZ_NE] = "ne",
    [IFZ_GT] = "gt",
    [IFZ_LT] = "lt",
    [IFZ_GE] = "ge",
    [IFZ_LE] = "le",
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
    } else {
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
    for (int i = 0; i < BO_COUNT; i++) {
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
    if (!type_obj) {
        fprintf(stderr, "Return instruction missing or invalid 'type' field\n");
        return 1;
    }

    char* type = NULL;
    if (cJSON_IsNull(type_obj)) {
        type = "null";
    } else {
        type = cJSON_GetStringValue(type_obj);
    }

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

static int parse_ifz(IfzOP* ifz, cJSON* instruction_json)
{
    cJSON* condition_obj = cJSON_GetObjectItem(instruction_json, "condition");
    if (!condition_obj || !cJSON_IsString(condition_obj)) {
        fprintf(stderr, "Ifz instruction missing or invalid 'condition' field\n");
        return 1;
    }
    char* condition = cJSON_GetStringValue(condition_obj);

    ifz->condition = -1;
    for (int i = 0; i < IFZ_CONDITION_COUNT; i++) {
        if (strcmp(condition, condition_signature[i]) == 0) {
            ifz->condition = i;
            break;
        }
    }

    if (ifz->condition < 0) {
        fprintf(stderr, "Ifz condition not known: %s\n", condition);
        return 1;
    }

    cJSON* target_obj = cJSON_GetObjectItem(instruction_json, "target");
    if (!target_obj || !cJSON_IsNumber(target_obj)) {
        fprintf(stderr, "Ifz instruction missing or invalid 'target' field\n");
        return 2;
    }

    ifz->target = cJSON_GetNumberValue(target_obj);

    return 0;
}

static int parse_get(GetOP* get, cJSON* instruction_json)
{
    return 0;
}

static int parse_throw(ThrowOP* trw, cJSON* instruction_json)
{
    return 0;
}

static Instruction*
parse_instruction(cJSON* instruction_json)
{
    Instruction* instruction = malloc(sizeof(Instruction));
    if (!instruction) {
        goto cleanup;
    }

    if (parse_opcode(&instruction->opcode, instruction_json)) {
        goto cleanup;
    }

    fprintf(stderr, "OPCODE: %s\n", opcode_print(instruction->opcode));

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
        if (parse_get(&instruction->data.get, instruction_json)) {
            goto cleanup;
        }
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
    case OP_IF_ZERO:
        if (parse_ifz(&instruction->data.ifz, instruction_json)) {
            goto cleanup;
        }
        break;
    case OP_NEW:
    case OP_DUP:
    case OP_INVOKE:
    case OP_THROW:
        if (parse_throw(&instruction->data.trw, instruction_json)) {
            goto cleanup;
        }
        break;
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
    int i = 0;
    cJSON_ArrayForEach(buffer, bytecode)
    {
        Instruction* instruction = parse_instruction(buffer);
        instruction_table->count++;
        instruction->seq = i;
        if (!instruction) {
            goto cleanup;
        }

        instruction_table->instructions[i] = instruction;
        i++;
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
        // free(instruction_table->offsetmap);
    }

    free(instruction_table);
}

char* opcode_print(Opcode opcode)
{
    char* result = NULL;

    switch (opcode) {
    case OP_LOAD:
        result = "OP_LOAD";
        break;
    case OP_PUSH:
        result = "OP_PUSH";
        break;
    case OP_BINARY:
        result = "OP_BINARY";
        break;
    case OP_GET:
        result = "OP_GET";
        break;
    case OP_RETURN:
        result = "OP_RETURN";
        break;
    case OP_IF_ZERO:
        result = "OP_IF_ZERO";
        break;
    case OP_NEW:
        result = "OP_NEW";
        break;
    case OP_DUP:
        result = "OP_DUP";
        break;
    case OP_INVOKE:
        result = "OP_INVOKE";
        break;
    case OP_THROW:
        result = "OP_THROW";
        break;
    default:
        break;
    }

    return result;
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

Value value_deep_copy(const Value* src)
{
    Value dst;
    dst.type = TYPE_VOID;

    if (!src)
        return dst;

    dst.type = src->type;

    switch (src->type) {
    case TYPE_INT:
        dst.data.int_value = src->data.int_value;
        break;

    case TYPE_BOOLEAN:
        dst.data.bool_value = src->data.bool_value;
        break;

    case TYPE_CHAR:
        dst.data.char_value = src->data.char_value;
        break;

    case TYPE_ARRAY:
        if (src->data.array_value) {
            int count = 0;
            while (src->data.array_value[count].type != TYPE_VOID) {
                count++;
            }

            dst.data.array_value = malloc((count + 1) * sizeof(Value));
            if (!dst.data.array_value) {
                fprintf(stderr, "Failed to allocate array\n");
                dst.type = TYPE_VOID;
                break;
            }

            for (int i = 0; i < count; i++) {
                dst.data.array_value[i] = value_deep_copy(&src->data.array_value[i]);
            }

            dst.data.array_value[count].type = TYPE_VOID;
        } else {
            dst.data.array_value = NULL;
        }
        break;

    case TYPE_REFERENCE:
        dst.data.ref_value = src->data.ref_value;
        break;

    case TYPE_VOID:
        break;

    default:
        fprintf(stderr, "Unknown value type: %d\n", src->type);
        dst.type = TYPE_VOID;
        break;
    }

    return dst;
}

int value_add(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return 1;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value + value2->data.int_value;
        break;
    default:
        fprintf(stderr, "Dont know handle this type add\n");
        return 2;
    }

    return 0;
}

int value_mul(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return 1;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value * value2->data.int_value;
        break;
    default:
        fprintf(stderr, "Dont know handle this type mul\n");
        return 2;
    }

    return 0;
}

int value_sub(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return 1;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value - value2->data.int_value;
        break;
    default:
        fprintf(stderr, "Dont know handle this type sub\n");
        return 2;
    }

    return 0;
}

int value_div(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return 1;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        if (value2->data.int_value == 0) {
            return 3;
        }

        result->data.int_value = value1->data.int_value / value2->data.int_value;
        break;
    default:
        fprintf(stderr, "Dont know handle this type div\n");
        return 2;
    }

    return 0;
}
