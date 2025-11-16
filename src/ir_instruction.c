#include "ir_instruction.h"
#include "cJSON/cJSON.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    IPR_OK,
    IPR_MALFORMED,
    IPR_ALLOC_ERROR,
    IPR_UNABLE_TO_HANDLE_TYPE,
    IPR_IF_COND_NOW_KNOWN,
    IPR_BO_UNKNOWN_OP,
} IrInstructionParseResult;

typedef IrInstructionParseResult (*IrInstructionParseHandler)(IrInstruction*, cJSON*);

static const char* load_type_signature[] = {
    [TK_INT] = "int",
    [TK_BOOLEAN] = "boolean",
    [TK_REFERENCE] = "ref",
};

static const char* push_type_signature[] = {
    [TK_INT] = "integer",
    [TK_BOOLEAN] = "boolean",
    [TK_VOID] = "null",
};

static const char* binary_type_signature[] = {
    [TK_INT] = "int",
};

static const char* store_type_signature[] = {
    [TK_INT] = "int",
    [TK_CHAR] = "char",
    [TK_REFERENCE] = "ref",
};

static const char* array_type_signature[] = {
    [TK_INT] = "int",
    [TK_CHAR] = "char",
};

static const char* return_type_signature[] = {
    [TK_INT] = "int",
    [TK_VOID] = "null",
    [TK_REFERENCE] = "ref",
};

static const char* invoke_args_type_signature[] = {
    [TK_INT] = "int",
    [TK_BOOLEAN] = "boolean",
    [TK_ARRAY] = "array",
};

static const char* binary_operator_signature[]
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

static IrInstructionParseResult parse_load(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    LoadOP* load = &ir_instruction->data.load;
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

    if (strcmp(type, load_type_signature[TK_INT]) == 0) {
        load->type = TYPE_INT;
    } else if (strcmp(type, load_type_signature[TK_BOOLEAN]) == 0) {
        load->type = TYPE_BOOLEAN;
    } else if (strcmp(type, load_type_signature[TK_REFERENCE]) == 0) {
        load->type = TYPE_REFERENCE;
    } else {
        LOG_ERROR("Unknown type in load instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_push(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    PushOP* push = &ir_instruction->data.push;
    cJSON* value_obj = cJSON_GetObjectItem(instruction_json, "value");
    if (!value_obj) {
        LOG_ERROR("Load instruction missing or invalid 'value' field");
        return IPR_MALFORMED;
    }

    cJSON* type_obj = cJSON_GetObjectItem(value_obj, "type");
    char* type;
    if (!type_obj) {
        type = "null";
    } else if (cJSON_IsString(type_obj)) {
        type = cJSON_GetStringValue(type_obj);
    } else {
        LOG_ERROR("Push instruction missing or invalid 'type' field: %s", cJSON_Print(type_obj));
        return IPR_MALFORMED;
    }

    if (strcmp(type, push_type_signature[TK_INT]) == 0) {
        cJSON* inside_value_obj = cJSON_GetObjectItem(value_obj, "value");
        if (!inside_value_obj || !cJSON_IsNumber(inside_value_obj)) {
            LOG_ERROR("Push instruction missing or invalid inside value, 'value' field");
            return IPR_MALFORMED;
        }
        char* inside_value = cJSON_GetStringValue(inside_value_obj);

        push->value.type = TYPE_INT;
        if (!cJSON_IsNumber(inside_value_obj)) {
            LOG_ERROR("Push instruction invalid int number inside value, 'value' field");
            return IPR_MALFORMED;
        }
        push->value.data.int_value = cJSON_GetNumberValue(inside_value_obj);
    } else if (strcmp(type, push_type_signature[TK_VOID]) == 0) {
        push->value.type = TYPE_REFERENCE;
        push->value.data.ref_value = 0;
    } else {
        LOG_ERROR("Unknown type in push instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_binary(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    BinaryOP* binary = &ir_instruction->data.binary;
    cJSON* operant_obj = cJSON_GetObjectItem(instruction_json, "operant");
    if (!operant_obj || !cJSON_IsString(operant_obj)) {
        LOG_ERROR("Binary instruction missing or invalid 'operant' field");
        return IPR_MALFORMED;
    }
    char* operant = cJSON_GetStringValue(operant_obj);

    binary->op = -1;
    for (int i = 0; i < BO_COUNT; i++) {
        if (strcmp(operant, binary_operator_signature[i]) == 0) {
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

    if (strcmp(binary_type_signature[TK_INT], type) == 0) {
        binary->type = TYPE_INT;
    } else {
        LOG_ERROR("Binary instruction unknown type: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_return(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    ReturnOP* ret = &ir_instruction->data.ret;
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

    if (strcmp(return_type_signature[TK_INT], type) == 0) {
        ret->type = TYPE_INT;
    } else if (strcmp(return_type_signature[TK_VOID], type) == 0) {
        ret->type = TYPE_VOID;
    } else if (strcmp(return_type_signature[TK_REFERENCE], type) == 0) {
        ret->type = TYPE_REFERENCE;
    } else {
        LOG_ERROR("Return instruction unknown type: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_ift(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    IfOP* ift = &ir_instruction->data.ift;
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

static IrInstructionParseResult parse_get(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    GetOP* get = &ir_instruction->data.get;
    return IPR_OK;
}

static IrInstructionParseResult parse_invoke(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    InvokeOP* invoke = &ir_instruction->data.invoke;
    cJSON* method_obj = cJSON_GetObjectItem(instruction_json, "method");
    if (!method_obj || !cJSON_IsObject(method_obj)) {
        LOG_ERROR("Invoke instruction missing or invalid 'method' field");
        return IPR_MALFORMED;
    }

    cJSON* name_obj = cJSON_GetObjectItem(method_obj, "name");
    if (!name_obj || !cJSON_IsString(name_obj)) {
        LOG_ERROR("Invoke instruction missing or invalid 'name' field");
        return IPR_MALFORMED;
    }
    invoke->method_name = strdup(cJSON_GetStringValue(name_obj));

    cJSON* ref_obj = cJSON_GetObjectItem(method_obj, "ref");
    if (!ref_obj || !cJSON_IsObject(ref_obj)) {
        LOG_ERROR("Invoke instruction missing or invalid 'ref' field");
        return IPR_MALFORMED;
    }
    cJSON* ref_name_obj = cJSON_GetObjectItem(ref_obj, "name");
    if (!ref_name_obj || !cJSON_IsString(ref_name_obj)) {
        LOG_ERROR("Invoke instruction missing or invalid 'name' field in 'ref'");
        return IPR_MALFORMED;
    }
    invoke->ref_name = strdup(cJSON_GetStringValue(ref_name_obj));

    cJSON* args_obj = cJSON_GetObjectItem(method_obj, "args");
    if (!args_obj || !cJSON_IsArray(args_obj)) {
        LOG_ERROR("Invoke instruction missing or invalid 'args' field");
        return IPR_MALFORMED;
    }

    int capacity = 10;
    invoke->args = malloc(sizeof(Type*) * capacity);
    invoke->args_len = 0;

    cJSON* buffer;
    cJSON_ArrayForEach(buffer, args_obj)
    {
        if (!cJSON_IsString(buffer)) {
            LOG_ERROR("Invoke instruction missing or invalid 'args' field");
            return IPR_MALFORMED;
        }

        char* type = cJSON_GetStringValue(buffer);

        Type* to_add;

        if (strcmp(type, invoke_args_type_signature[TK_INT]) == 0) {
            to_add = TYPE_INT;
        } else if (strcmp(type, invoke_args_type_signature[TK_BOOLEAN]) == 0) {
            to_add = TYPE_BOOLEAN;
        } else {
            LOG_ERROR("Invoke instruction not supported type in 'args' field: %s", type);
            return IPR_MALFORMED;
        }

        if (capacity <= invoke->args_len) {
            capacity *= 2;
            invoke->args = realloc(invoke->args, sizeof(Type*) * capacity);
        }

        invoke->args[invoke->args_len] = to_add;
        invoke->args_len++;
    }

    if (invoke->args_len) {
        invoke->args = realloc(invoke->args, invoke->args_len * sizeof(Type*));
        if (!invoke->args) {
            return IPR_ALLOC_ERROR;
        }
    } else {
        invoke->args = NULL;
    }

    cJSON* returns_obj = cJSON_GetObjectItem(method_obj, "returns");
    if (!returns_obj) {
        LOG_ERROR("Invoke instruction missing or invalid 'returns' field");
        return IPR_MALFORMED;
    }

    if (cJSON_IsNull(returns_obj)) {
        invoke->return_type = TYPE_VOID;
    } else if (cJSON_IsString(returns_obj)) {
        char* type = cJSON_GetStringValue(returns_obj);
        if (strcmp(type, invoke_args_type_signature[TK_INT]) == 0) {
            invoke->return_type = TYPE_INT;
        } else {
            LOG_ERROR("Unable to handle invoke instruction 'returns' type");
            return IPR_MALFORMED;
        }
    } else if (cJSON_IsObject(returns_obj)) {
        cJSON* return_kind = cJSON_GetObjectItem(returns_obj, "kind");
        cJSON* return_type = cJSON_GetObjectItem(returns_obj, "type");
        if (!return_kind || !return_type || !cJSON_IsString(return_kind) || !cJSON_IsString(return_type)) {
            LOG_ERROR("Unable to handle invoke instruction 'returns' field: %s", cJSON_Print(returns_obj));
            return IPR_MALFORMED;
        }

        char* kind = cJSON_GetStringValue(return_kind);
        char* type = cJSON_GetStringValue(return_type);

        if (strcmp(kind, invoke_args_type_signature[TK_ARRAY]) == 0) {
            if (strcmp(type, invoke_args_type_signature[TK_INT]) == 0) {
                invoke->return_type = make_array_type(TYPE_INT);
            } else {
                LOG_ERROR("Unable to handle invoke instruction 'returns' field: %s", cJSON_Print(returns_obj));
                return IPR_MALFORMED;
            }
        } else {
            LOG_ERROR("Unable to handle invoke instruction 'returns' field: %s", cJSON_Print(returns_obj));
            return IPR_MALFORMED;
        }
    } else {
        LOG_ERROR("Unable to handle invoke instruction 'returns' field: %s", cJSON_Print(returns_obj));
        return IPR_MALFORMED;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_throw(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    ThrowOP* trw = &ir_instruction->data.trw;
    return IPR_OK;
}

static IrInstructionParseResult parse_dup(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    DupOP* dup = &ir_instruction->data.dup;
    cJSON* words_obj = cJSON_GetObjectItem(instruction_json, "words");
    if (!words_obj || !cJSON_IsNumber(words_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'words' field");
        return IPR_MALFORMED;
    }
    dup->words = cJSON_GetNumberValue(words_obj);

    return IPR_OK;
}

static IrInstructionParseResult parse_store(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    StoreOP* store = &ir_instruction->data.store;
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

    if (strcmp(type, store_type_signature[TK_INT]) == 0) {
        store->type = TYPE_INT;
    } else if (strcmp(type, store_type_signature[TK_CHAR]) == 0) {
        store->type = TYPE_CHAR;
    } else if (strcmp(type, store_type_signature[TK_REFERENCE]) == 0) {
        store->type = TYPE_REFERENCE;
    } else {
        LOG_ERROR("Unknown type in store instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_goto(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    GotoOP* go2 = &ir_instruction->data.go2;
    cJSON* target_obj = cJSON_GetObjectItem(instruction_json, "target");
    if (!target_obj || !cJSON_IsNumber(target_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'target' field");
        return IPR_MALFORMED;
    }
    go2->target = cJSON_GetNumberValue(target_obj);

    return IPR_OK;
}

static IrInstructionParseResult parse_cast(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    CastOP* cast = &ir_instruction->data.cast;
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

static IrInstructionParseResult parse_new_array(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    NewArrayOP* new_array = &ir_instruction->data.new_array;
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

    // todo: handle multi dimensional array
    if (strcmp(type, array_type_signature[TK_INT]) == 0) {
        new_array->type = TYPE_INT;
    } else if (strcmp(type, array_type_signature[TK_CHAR]) == 0) {
        new_array->type = TYPE_CHAR;
    } else {
        LOG_ERROR("Unknown type in newarray instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_array_load(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    ArrayLoadOP* array_load = &ir_instruction->data.array_load;
    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, array_type_signature[TK_INT]) == 0) {
        array_load->type = TYPE_INT;
    } else if (strcmp(type, array_type_signature[TK_CHAR]) == 0) {
        array_load->type = TYPE_CHAR;
    } else {
        LOG_ERROR("Unknown type in array load instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_array_store(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    ArrayStoreOP* array_store = &ir_instruction->data.array_store;
    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }
    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, array_type_signature[TK_INT]) == 0) {
        array_store->type = TYPE_INT;
    } else if (strcmp(type, array_type_signature[TK_CHAR]) == 0) {
        array_store->type = TYPE_CHAR;
    } else {
        LOG_ERROR("Unknown type in array store instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseResult parse_array_length(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    ArrayLengthOP* array_length = &ir_instruction->data.array_length;
    return IPR_OK;
}

static IrInstructionParseResult parse_incr(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    IncrOP* incr = &ir_instruction->data.incr;
    cJSON* index_obj = cJSON_GetObjectItem(instruction_json, "index");
    if (!index_obj || !cJSON_IsNumber(index_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'index' field");
        return IPR_MALFORMED;
    }
    incr->index = cJSON_GetNumberValue(index_obj);

    cJSON* amount_obj = cJSON_GetObjectItem(instruction_json, "amount");
    if (!amount_obj || !cJSON_IsNumber(amount_obj)) {
        LOG_ERROR("Parse instruction missing or invalid 'amount' field");
        return IPR_MALFORMED;
    }
    incr->amount = cJSON_GetNumberValue(amount_obj);

    return IPR_OK;
}

static IrInstructionParseResult parse_negate(IrInstruction* ir_instruction, cJSON* instruction_json)
{
    NegateOP* negate = &ir_instruction->data.negate;

    cJSON* type_obj = cJSON_GetObjectItem(instruction_json, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        LOG_ERROR("Load instruction missing or invalid 'type' field");
        return IPR_MALFORMED;
    }

    char* type = cJSON_GetStringValue(type_obj);

    if (strcmp(type, load_type_signature[TK_INT]) == 0) {
        negate->type = TYPE_INT;
    } else if (strcmp(type, load_type_signature[TK_BOOLEAN]) == 0) {
        negate->type = TYPE_BOOLEAN;
    } else if (strcmp(type, load_type_signature[TK_REFERENCE]) == 0) {
        negate->type = TYPE_REFERENCE;
    } else {
        LOG_ERROR("Unknown type in negate instruction: %s", type);
        return IPR_UNABLE_TO_HANDLE_TYPE;
    }

    return IPR_OK;
}

static IrInstructionParseHandler ir_instruction_table[OP_COUNT] = {
    [OP_LOAD] = parse_load,
    [OP_PUSH] = parse_push,
    [OP_BINARY] = parse_binary,
    [OP_GET] = parse_get,
    [OP_RETURN] = parse_return,
    [OP_IF_ZERO] = parse_ift,
    [OP_IF] = parse_ift,
    [OP_DUP] = parse_dup,
    [OP_INVOKE] = parse_invoke,
    [OP_NEW] = parse_throw,
    [OP_CAST] = parse_cast,
    [OP_THROW] = parse_throw,
    [OP_STORE] = parse_store,
    [OP_GOTO] = parse_goto,
    [OP_NEW_ARRAY] = parse_new_array,
    [OP_ARRAY_LENGTH] = parse_array_length,
    [OP_ARRAY_STORE] = parse_array_store,
    [OP_ARRAY_LOAD] = parse_array_load,
    [OP_INCR] = parse_incr,
    [OP_NEGATE] = parse_negate,
};

IrInstruction*
ir_instruction_parse(cJSON* instruction_json)
{
    IrInstruction* ir_instruction = malloc(sizeof(IrInstruction));
    if (!ir_instruction) {
        goto cleanup;
    }

    ir_instruction->opcode = opcode_parse(instruction_json);
    if (ir_instruction->opcode < 0 || ir_instruction->opcode >= OP_COUNT) {
        LOG_ERROR("Unknown opcode: %d", ir_instruction->opcode);
        goto cleanup;
    }

    LOG_DEBUG("Parsing opcode: %s", opcode_print(ir_instruction->opcode));

    if (ir_instruction_table[ir_instruction->opcode](ir_instruction, instruction_json)) {
        goto cleanup;
    }

    return ir_instruction;

cleanup:
    free(ir_instruction);
    return NULL;
}

int ir_instruction_is_conditional(IrInstruction* ir_instruction)
{
    return ir_instruction->opcode == OP_IF || ir_instruction->opcode == OP_IF_ZERO;
}
