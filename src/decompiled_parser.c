#include "decompiled_parser.h"
#include "log.h"

#include "cJSON/cJSON.h"

#include <stdlib.h>
#include <string.h>

char args_type_signature[] = {
    [TYPE_INT] = 'I',
    [TYPE_CHAR] = 'C',
    [TYPE_ARRAY] = '[',
    [TYPE_BOOLEAN] = 'Z',
};

typedef enum {
    IPR_OK,
    IPR_MALFORMED,
    IPR_UNABLE_TO_HANDLE_TYPE,
    IPR_IF_COND_NOW_KNOWN,
    IPR_BO_UNKNOWN_OP,
} InstructionParseResult;

static const char* opcode_signature[] = {
    [OP_LOAD] = "load",
    [OP_PUSH] = "push",
    [OP_BINARY] = "binary",
    [OP_GET] = "get",
    [OP_RETURN] = "return",
    [OP_IF_ZERO] = "ifz",
    [OP_IF] = "if",
    [OP_NEW] = "new",
    [OP_DUP] = "dup",
    [OP_INVOKE] = "invoke",
    [OP_THROW] = "throw",
    [OP_STORE] = "store",
    [OP_GOTO] = "goto",
    [OP_CAST] = "cast",
    [OP_NEW_ARRAY] = "newarray",
    [OP_ARRAY_LOAD] = "array_load",
    [OP_ARRAY_STORE] = "array_store",
    [OP_ARRAY_LENGTH] = "arraylength",
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

static const char* store_type_signature[] = {
    [TYPE_INT] = "int",
};

static const char* array_type_signature[] = {
    [TYPE_INT] = "int",
};

static const char* return_type_signature[] = {
    [TYPE_INT] = "int",
    [TYPE_VOID] = "null",
};

static const char* invoke_args_type_signature[] = {
    [TYPE_INT] = "int",
};

static const char* binary_operatore_signature[]
    = {
          [BO_ADD] = "add",
          [BO_SUB] = "sub",
          [BO_DIV] = "div",
          [BO_MUL] = "mul",
          [BO_REM] = "rem",
      };

static const char* condition_signature[] = {
    [IF_EQ] = "eq",
    [IF_NE] = "ne",
    [IF_GT] = "gt",
    [IF_LT] = "lt",
    [IF_GE] = "ge",
    [IF_LE] = "le",
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

static InstructionParseResult parse_opcode(Opcode* opcode, cJSON* instruction_json)
{
    cJSON* opr_object = cJSON_GetObjectItem(instruction_json, "opr");
    if (!opr_object || !cJSON_IsString(opr_object)) {
        LOG_ERROR("Opr is wrongly formatted or not exist");
        return IPR_MALFORMED;
    }

    char* opr = cJSON_GetStringValue(opr_object);

    *opcode = -1;
    for (int i = 0; i < OP_COUNT; i++) {
        if (strcmp(opr, opcode_signature[i]) == 0) {
            *opcode = i;
        }
    }

    if ((*opcode) == -1) {
        LOG_ERROR("Not known opr: %s", opr);
        return 2;
    }

    return IPR_OK;
}

static InstructionParseResult parse_load(LoadOP* load, cJSON* instruction_json)
{
    cJSON* index_obj = cJSON_GetObjectItem(instruction_json, "index");
    if (!index_obj || !cJSON_IsNumber(index_obj)) {
        LOG_ERROR("Load instruction missing or invalid 'index' field");
        return IPR_MALFORMED;
    }
    load->index = (int)cJSON_GetNumberValue(index_obj);

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Load instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }

    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, load_type_signature[TYPE_INT]) == 0) {
        load->type = TYPE_INT;
    } else if (strcmp(type, load_type_signature[TYPE_BOOLEAN]) == 0) {
        load->type = TYPE_BOOLEAN;
    } else if (strcmp(type, load_type_signature[TYPE_REFERENCE]) == 0) {
        load->type = TYPE_REFERENCE;
    } else {
        LOG_ERROR("Unknown type in load instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_push(PushOP* push, cJSON* instruction_json)
{
    cJSON* value = cJSON_GetObjectItem(instruction_json, "value");
    if (!value) {
        LOG_ERROR("Load instruction missing or invalid 'value' field");
        return IPR_MALFORMED;
    }

    cJSON* type_obj = cJSON_GetObjectItem(value, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Push instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    cJSON* inside_TYPE_obj = cJSON_GetObjectItem(value, "value");
    if (!inside_TYPE_obj) {
        LOG_ERROR("Push instruction missing or invalid inside value, 'value' field");
        return IPR_MALFORMED;
    }
    char* inside_value = cJSON_GetStringValue(inside_TYPE_obj);

    if (strcmp(type, push_type_signature[TYPE_INT]) == 0) {
        push->value.type = TYPE_INT;
        if (!cJSON_IsNumber(inside_TYPE_obj)) {
            LOG_ERROR("Push instruction invalid int number inside value, 'value' field");
            return IPR_MALFORMED;
        }
        push->value.data.int_value = cJSON_GetNumberValue(inside_TYPE_obj);
    } else {
        LOG_ERROR("Unknown type in push instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_binary(BinaryOP* binary, cJSON* instruction_json)
{
    cJSON* operant_obj = cJSON_GetObjectItem(instruction_json, "operant");
    if (!operant_obj || !cJSON_IsString(operant_obj)) {
        LOG_ERROR("Binary instruction missing or invalid 'operant' field");
        return IPR_MALFORMED;
    }
    char* operant = cJSON_GetStringValue(operant_obj);

    binary->op = -1;
    for (int i = 0; i < BO_COUNT; i++) {
        if (strcmp(operant, binary_operatore_signature[i]) == 0) {
            binary->op = i;
            break;
        }
    }

    if (binary->op == -1) {
        LOG_ERROR("Binary instruction unknown operant: %s", operant);
        return IPR_BO_UNKNOWN_OP;
    }

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Binary instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(binary_type_signature[TYPE_INT], type) == 0) {
        binary->type = TYPE_INT;
    } else {
        LOG_ERROR("Binary instruction unknown type: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_return(ReturnOP* ret, cJSON* instruction_json)
{
    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj) {
        LOG_ERROR("Return instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
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
        LOG_ERROR("Binary instruction unknown type: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_ift(IfOP* ift, cJSON* instruction_json)
{
    cJSON* condition_obj = cJSON_GetObjectItem(instruction_json, "condition");
    if (!condition_obj || !cJSON_IsString(condition_obj)) {
        LOG_ERROR("If instruction missing or invalid 'condition' field");
        return IPR_MALFORMED;
    }
    char* condition = cJSON_GetStringValue(condition_obj);

    ift->condition = -1;
    for (int i = 0; i < IF_CONDITION_COUNT; i++) {
        if (strcmp(condition, condition_signature[i]) == 0) {
            ift->condition = i;
            break;
        }
    }

    if (ift->condition < 0) {
        LOG_ERROR("If condition not known: %s", condition);
        return IPR_IF_COND_NOW_KNOWN;
    }

    cJSON* target_obj = cJSON_GetObjectItem(instruction_json, "target");
    if (!target_obj || !cJSON_IsNumber(target_obj)) {
        LOG_ERROR("If instruction missing or invalid 'target' field");
        return IPR_MALFORMED;
    }

    ift->target = cJSON_GetNumberValue(target_obj);

    return IPR_OK;
}

static InstructionParseResult parse_get(GetOP* get, cJSON* instruction_json)
{
    return IPR_OK;
}

// static InstructionParseResult parse_invoke(InvokeOP* invoke, cJSON* instruction_json)
//{
//     cJSON* method_obj = cJSON_GetObjectItem(instruction_json, "method");
//     if (!method_obj || !cJSON_IsObject(method_obj)) {
//LOG_ERROR("Invoke instruction missing or invalid 'method' field");
//         return IPR_MALFORMED;
//     }
//
//     cJSON* name_obj = cJSON_GetObjectItem(method_obj, "name");
//     if (!name_obj || !cJSON_IsString(name_obj)) {
//LOG_ERROR("Invoke instruction missing or invalid 'name' field");
//         return IPR_MALFORMED;
//     }
//     invoke->method_name = cJSON_GetStringValue(name_obj);
//
//     cJSON* ref_obj = cJSON_GetObjectItem(method_obj, "ref");
//     if (!ref_obj || !cJSON_IsObject(ref_obj)) {
//LOG_ERROR("Invoke instruction missing or invalid 'ref' field");
//         return IPR_MALFORMED;
//     }
//     cJSON* ref_name_obj = cJSON_GetObjectItem(method_obj, "name");
//     if (!ref_name_obj || !cJSON_IsString(ref_name_obj)) {
//LOG_ERROR("Invoke instruction missing or invalid 'name' field in 'ref'");
//         return IPR_MALFORMED;
//     }
//     invoke->ref_name = cJSON_GetStringValue(ref_name_obj);
//
//     cJSON* args_obj = cJSON_GetObjectItem(method_obj, "args");
//     if (!args_obj || !cJSON_IsArray(args_obj)) {
//LOG_ERROR("Invoke instruction missing or invalid 'args' field");
//         return IPR_MALFORMED;
//     }
//
//     // todo: handle args dynamically
//     char args[256] = "";
//     int args_len = 0;
//
//     cJSON* buffer;
//     cJSON_ArrayForEach(buffer, args_obj)
//     {
//         if (!cJSON_IsString(buffer)) {
//LOG_ERROR("Invoke instruction missing or invalid 'args' field");
//             return IPR_MALFORMED;
//         }
//
//         char* type = cJSON_GetStringValue(buffer);
//
//         if (strcmp(type, invoke_args_type_signature[TYPE_INT]) == 0) {
//             args[args_len] = args_type_signature[TYPE_INT];
//             args_len++;
//         } else {
//LOG_ERROR("Invoke instruction not supported type in 'args' field");
//             return IPR_MALFORMED;
//         }
//     }
//
//     args[args_len] = '\0';
//
//     // todo: free after use while deleting instruction table
//     invoke->args = strdup(args);
//
//     cJSON* returns_obj = cJSON_GetObjectItem(method_obj, "returns");
//     if (!returns_obj) {
//LOG_ERROR("Invoke instruction missing or invalid 'returns' field");
//         return IPR_MALFORMED;
//     }
//
//     if (cJSON_IsNull(returns_obj)) {
//     }
//
//     return IPR_OK;
// }

static InstructionParseResult parse_throw(ThrowOP* trw, cJSON* instruction_json)
{
    return IPR_OK;
}

static InstructionParseResult parse_dup(DupOP* dup, cJSON* instruction_json)
{
    cJSON* words_obj = cJSON_GetObjectItem(instruction_json, "words");
    if (!words_obj || !cJSON_IsNumber(words_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'words' field");
        return IPR_MALFORMED;
    }
    dup->words = cJSON_GetNumberValue(words_obj);

    return IPR_OK;
}

static InstructionParseResult parse_store(StoreOP* store, cJSON* instruction_json)
{
    cJSON* index_obj = cJSON_GetObjectItem(instruction_json, "index");
    if (!index_obj || !cJSON_IsNumber(index_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'index' field");
        return IPR_MALFORMED;
    }
    store->index = cJSON_GetNumberValue(index_obj);

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, store_type_signature[TYPE_INT]) == 0) {
        store->type = TYPE_INT;
    } else {
        LOG_ERROR("Unknown type in store instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_goto(GotoOP* go2, cJSON* instruction_json)
{
    cJSON* target_obj = cJSON_GetObjectItem(instruction_json, "target");
    if (!target_obj || !cJSON_IsNumber(target_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'target' field");
        return IPR_MALFORMED;
    }
    go2->target = cJSON_GetNumberValue(target_obj);

    return IPR_OK;
}

static InstructionParseResult parse_cast(CastOP* cast, cJSON* instruction_json)
{
    cJSON* from_obj = cJSON_GetObjectItem(instruction_json, "from");
    if (!from_obj || !cJSON_IsString(from_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'from' field");
        return IPR_MALFORMED;
    }
    char* from = cJSON_GetStringValue(from_obj);
    // todo: convert to ValueType

    cJSON* to_obj = cJSON_GetObjectItem(instruction_json, "to");
    if (!to_obj || !cJSON_IsString(to_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'to' field");
        return IPR_MALFORMED;
    }
    char* to = cJSON_GetStringValue(to_obj);
    // todo: convert to ValueType

    return IPR_OK;
}

static InstructionParseResult parse_new_array(NewArrayOP* new_array, cJSON* instruction_json)
{
    cJSON* dim_obj = cJSON_GetObjectItem(instruction_json, "dim");
    if (!dim_obj || !cJSON_IsNumber(dim_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'dim' field");
        return IPR_MALFORMED;
    }
    new_array->dim = cJSON_GetNumberValue(dim_obj);

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, array_type_signature[TYPE_INT]) == 0) {
        new_array->type = TYPE_INT;
    } else {
        LOG_ERROR("Unknown type in newarray instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_array_load(ArrayLoadOP* load, cJSON* instruction_json)
{
    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, array_type_signature[TYPE_INT]) == 0) {
        load->type = TYPE_INT;
    } else {
        LOG_ERROR("Unknown type in newarray instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_array_store(ArrayStoreOP* store, cJSON* instruction_json)
{
    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, array_type_signature[TYPE_INT]) == 0) {
        store->type = TYPE_INT;
    } else {
        LOG_ERROR("Unknown type in newarray instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static InstructionParseResult parse_array_length(ArrayLengthOP* array_length, cJSON* instruction_json)
{
    return IPR_OK;
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

    LOG_DEBUG("Parsing opcode: %s", opcode_print(instruction->opcode));

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
        if (parse_ift(&instruction->data.ift, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_IF:
        if (parse_ift(&instruction->data.ift, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_DUP:
        if (parse_dup(&instruction->data.dup, instruction_json)) {
            goto cleanup;
        }
        break;
    case OP_NEW:
    case OP_INVOKE:
    case OP_THROW:
        if (parse_throw(&instruction->data.trw, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_STORE:
        if (parse_store(&instruction->data.store, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_GOTO:
        if (parse_goto(&instruction->data.go2, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_CAST:
        if (parse_cast(&instruction->data.cast, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_NEW_ARRAY:
        if (parse_new_array(&instruction->data.new_array, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_ARRAY_LOAD:
        if (parse_array_load(&instruction->data.array_load, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_ARRAY_STORE:
        if (parse_array_store(&instruction->data.array_store, instruction_json)) {
            goto cleanup;
        }
        break;

    case OP_ARRAY_LENGTH:
        if (parse_array_length(&instruction->data.array_length, instruction_json)) {
            goto cleanup;
        }
        break;

    default:
        LOG_ERROR("Unknown opcode: %d", instruction->opcode);
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

const char* opcode_print(Opcode opcode)
{
    if (opcode < 0 && opcode >= OP_COUNT) {
        return NULL;
    }

    return opcode_signature[opcode];
}

// void value_print(const Value* value)
//{
//     if (value == NULL) {
//         printf("NULL");
//         return;
//     }
//
//     switch (value->type) {
//     case TYPE_INT:
//         printf("%d", value->data.int_value);
//         break;
//     case TYPE_BOOLEAN:
//         printf("%s", value->data.bool_value ? "true" : "false");
//         break;
//     case TYPE_ARRAY:
//         printf("[");
//         if (value->data.array_value != NULL) {
//             Value* arr = value->data.array_value;
//             for (int i = 0; arr[i].type != TYPE_VOID; i++) {
//                 if (i > 0)
//                     printf(", ");
//                 value_print(&arr[i]);
//             }
//         }
//         printf("]");
//         break;
//     case TYPE_VOID:
//         printf("void");
//         break;
//     default:
//         printf("unknown_value");
//         break;
//     }
// }

BinaryResult value_add(PrimitiveType* value1, PrimitiveType* value2, PrimitiveType* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value + value2->data.int_value;
        break;
    default:
        LOG_ERROR("Dont know handle this type add");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_mul(PrimitiveType* value1, PrimitiveType* value2, PrimitiveType* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value * value2->data.int_value;
        break;
    default:
        LOG_ERROR("Dont know handle this type mul");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_sub(PrimitiveType* value1, PrimitiveType* value2, PrimitiveType* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value - value2->data.int_value;
        break;
    default:
        LOG_ERROR("Dont know handle this type sub");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_rem(PrimitiveType* value1, PrimitiveType* value2, PrimitiveType* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value % value2->data.int_value;
        break;
    default:
        LOG_ERROR("Dont know handle this type sub");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_div(PrimitiveType* value1, PrimitiveType* value2, PrimitiveType* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    switch (value1->type) {
    case TYPE_INT:
        result->type = TYPE_INT;
        if (value2->data.int_value == 0) {
            return BO_DIVIDE_BY_ZERO;
        }

        result->data.int_value = value1->data.int_value / value2->data.int_value;
        break;
    default:
        LOG_ERROR("Dont know handle this type div");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}
