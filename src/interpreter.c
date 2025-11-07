// todo handle pop error, use vector
#include "interpreter.h"
#include "cli.h"
#include "heap.h"
#include "ir_function.h"
#include "ir_instruction.h"
#include "ir_program.h"
#include "log.h"
#include "opcode.h"
#include "stack.h"
#include "string.h"
#include "utils.h"
#include "value.h"

#include <stdlib.h>

#define ITERATION 100000

// todo: reason about splitting in specific code for each op
typedef enum {
    SR_OK,
    SR_OUT_OF_BOUNDS,
    SR_NULL_INSTRUCTION,
    SR_NULL_POINTER,
    SR_EMPTY_STACK,
    SR_DIVIDE_BY_ZERO,
    SR_BO_UNKNOWN,
    SR_BO_MUL_GEN_ERR,
    SR_BO_DIV_DBZ,
    SR_BO_DIV_GEN_ERR,
    SR_BO_ADD_GEN_ERR,
    SR_BO_SUB_GEN_ERR,
    SR_BO_REM_GEN_ERR,
    SR_UNKNOWN_OPCODE,
    SR_ASSERTION_ERR,
    SR_INVALID_TYPE,
    SR_INTERNAL_NULL_ERR,
} StepResult;

static const char* step_result_signature[] = {
    [SR_OK] = "SR_OK",
    [SR_OUT_OF_BOUNDS] = "SR_OUT_OF_BOUNDS",
    [SR_NULL_INSTRUCTION] = "SR_NULL_INSTRUCTION",
    [SR_NULL_POINTER] = "SR_NULL_POINTER",
    [SR_EMPTY_STACK] = "SR_EMPTY_STACK",
    [SR_DIVIDE_BY_ZERO] = "SR_DIVIDE_BY_ZERO",
    [SR_BO_UNKNOWN] = "SR_BO_UNKNOWN",
    [SR_BO_MUL_GEN_ERR] = "SR_BO_MUL_GEN_ERR",
    [SR_BO_DIV_DBZ] = "SR_BO_DIV_DBZ",
    [SR_BO_DIV_GEN_ERR] = "SR_BO_DIV_GEN_ERR",
    [SR_BO_ADD_GEN_ERR] = "SR_BO_ADD_GEN_ERR",
    [SR_BO_SUB_GEN_ERR] = "SR_BO_SUB_GEN_ERR",
    [SR_BO_REM_GEN_ERR] = "SR_BO_REM_GEN_ERR",
    [SR_UNKNOWN_OPCODE] = "SR_UNKNOWN_OPCODE",
    [SR_ASSERTION_ERR] = "SR_ASSERTION_ERR",
    [SR_INVALID_TYPE] = "SR_INVALID_TYPE",
    [SR_INTERNAL_NULL_ERR] = "SR_INTERNAL_NULL_ERR",
};

typedef struct {
    Value* locals;
    Stack* stack;
    IrFunction* ir_function;
    int locals_count;
    int pc;
} Frame;

typedef StepResult (*OpHandler)(VMContext*, IrInstruction*);

typedef struct CallStackNode {
    Frame* frame;
    struct CallStackNode* next;
} CallStackNode;

typedef struct {
    CallStackNode* top;
    int count;
} CallStack;

struct VMContext {
    Heap* heap;
    CallStack* call_stack;
    const Config* cfg;
    Frame* frame;
};

static void call_stack_push(CallStack* call_stack, Frame* frame)
{
    if (!call_stack || !frame)
        return;

    CallStackNode* new_node = malloc(sizeof(CallStackNode));
    if (!new_node) {
        LOG_ERROR("CallStack push: malloc failed");
        return;
    }

    new_node->frame = frame;
    new_node->next = call_stack->top;

    call_stack->top = new_node;
    call_stack->count++;
}

static Frame* call_stack_pop(CallStack* call_stack)
{
    if (!call_stack || !call_stack->top) {
        return NULL;
    }

    CallStackNode* to_pop = call_stack->top;
    Frame* popped_frame = to_pop->frame;

    call_stack->top = to_pop->next;
    call_stack->count--;

    free(to_pop);

    return popped_frame;
}

static Frame* call_stack_peek(CallStack* call_stack)
{
    if (!call_stack || !call_stack->top) {
        return NULL;
    }

    return call_stack->top->frame;
}

static void parameter_fix(char* parameters)
{
    bool open = false;

    while (*parameters != '\0') {
        if (*parameters == '[') {
            open = true;
        } else if (*parameters == ']') {
            open = false;
        }

        if (open) {
            if (*parameters == ',') {
                *parameters = ';';
            } else if (*parameters == ' ') {
                *parameters = '_';
            }
        }

        parameters++;
    }
}

// todo: handle any dimensional array
static int parse_array(Type* type, char* token, ObjectValue* array)
{
    char* values = strtok(token, ":_;[]");
    int capacity = 10;
    int* len = &array->array.elements_count;
    array->array.elements = malloc(sizeof(Value) * capacity);

    array->type = type;

    if (type == make_array_type(TYPE_INT)) {
        while ((values = strtok(NULL, ";[]_")) != NULL) {
            if (*len >= capacity) {
                capacity *= 2;
                array->array.elements = realloc(array->array.elements, sizeof(Value) * capacity);
            }

            Value value = { .type = TYPE_INT, .data.int_value = strtol(values, NULL, 10) };
            array->array.elements[(*len)] = value;

            (*len)++;
        }

        array->array.elements = realloc(array->array.elements, sizeof(Value) * (*len));
    } else if (type == make_array_type(TYPE_CHAR)) {
        while ((values = strtok(NULL, ";[]_'\"")) != NULL) {
            if (*len >= capacity) {
                capacity *= 2;
                array->array.elements = realloc(array->array.elements, sizeof(Value) * capacity);
            }

            Value value = { .type = TYPE_CHAR, .data.char_value = values[0] };
            array->array.elements[(*len)] = value;

            (*len)++;
        }

        array->array.elements = realloc(array->array.elements, sizeof(Value) * (*len));
    } else {
        LOG_ERROR("Dont know how to handle this array type interpreter.c: %s", token);
        return 1;
    }

    return 0;
}

static int parse_next_parameter(Heap* heap, char** arguments, char* token, Value* value)
{
    if (!arguments || !(*arguments) || !token || !value) {
        return 1;
    }

    Type* type = get_type(arguments);
    if (!type) {
        LOG_ERROR("Unknown arg type in method signature: %c", **arguments);
        return 2;
    }

    (*arguments)++;

    if (type_is_array(type)) {
        ObjectValue* array = malloc(sizeof(ObjectValue));

        if (parse_array(type, token, array)) {
            LOG_ERROR("While handling parse array");
            return 10;
        }

        value->type = TYPE_REFERENCE;

        if (heap_insert(heap, array, &value->data.ref_value)) {
            LOG_ERROR("While inserting array into heap");
            return 11;
        }
    } else if (type == TYPE_INT) {
        value->type = type;
        value->data.int_value = (int)strtol(token, NULL, 10);
    } else if (type == TYPE_BOOLEAN) {
        value->type = type;

        if (strcmp(token, "false") == 0) {
            value->data.bool_value = false;
        } else if (strcmp(token, "true") == 0) {
            value->data.bool_value = true;
        } else {
            LOG_ERROR("Type is bool but token is neither 'true' or 'false': %s", token);
            return 4;
        }
    } else {
        LOG_ERROR("Not handled type in method signature: %c", **arguments);
        return 3;
    }

    return 0;
}

static Value* build_locals_from_str(Heap* heap, const Method* m, char* parameters, int* locals_count)
{
    char* arguments = strdup(method_get_arguments(m));
    char* arguments_pointer = arguments;

    int capacity = 10;
    Value* locals = malloc(capacity * sizeof(Value));

    parameter_fix(parameters);
    char* strtok_state = NULL;
    char* token = strtok_r(parameters, "(), ", &strtok_state);
    while (token != NULL) {

        if (*locals_count >= capacity) {
            capacity *= 2;
            locals = realloc(locals, capacity * sizeof(Value));
            if (!locals) {
                return NULL;
            }
        }

        Value value;
        if (parse_next_parameter(heap, &arguments, token, &value)) {
            return NULL;
        }

        // todo check for int, ref, float
        locals[*locals_count] = value;

        token = strtok_r(NULL, "), ", &strtok_state);
        (*locals_count)++;
    }

    if (!(*locals_count)) {
        free(locals);
        locals = NULL;
    } else {
        locals = realloc(locals, *locals_count * sizeof(Value));
    }

    return locals;
}

// todo: handle pop error
static Value* build_locals_from_frame(const Method* m, Frame* frame, int* locals_count)
{
    char* arguments = strdup(method_get_arguments(m));
    char* arguments_pointer = arguments;

    int args_len = strlen(arguments);
    Value* locals = malloc(args_len * sizeof(Value));

    *locals_count = args_len;

    for (int i = strlen(arguments) - 1; i >= 0; i--) {
        stack_pop(frame->stack, &locals[i]);
    }

    free(arguments);

    return locals;
}

static Frame* build_frame(const Method* m, const Config* cfg, Value* locals, int locals_count)
{
    Frame* frame = malloc(sizeof(Frame));
    if (!frame) {
        return NULL;
    }

    frame->pc = 0;
    frame->stack = stack_new();
    frame->locals_count = locals_count;
    frame->locals = locals;
    frame->ir_function = ir_program_get_function(m, cfg);
    if (!frame->ir_function) {
        LOG_ERROR("Failed to build instruction table for method: %s", method_get_id(m));
        free(frame->stack);
        free(frame);
        return NULL;
    }

    return frame;
}

static StepResult handle_load(VMContext* vm_context, IrInstruction* ir_instruction)
{
    LoadOP* load = &ir_instruction->data.load;
    if (!load) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    int index = load->index;
    if (index >= frame->locals_count) {
        return SR_OUT_OF_BOUNDS;
    }

    Value value = frame->locals[index];
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_push(VMContext* vm_context, IrInstruction* ir_instruction)
{
    PushOP* push = &ir_instruction->data.push;
    if (!push) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value = push->value;
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_binary(VMContext* vm_context, IrInstruction* ir_instruction)
{
    BinaryOP* binary = &ir_instruction->data.binary;
    if (!binary) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value1, value2, result;
    if (stack_pop(frame->stack, &value2)) {
        return SR_EMPTY_STACK;
    }

    if (stack_pop(frame->stack, &value1)) {
        return SR_EMPTY_STACK;
    }

    switch (binary->op) {
    case BO_MUL:
        if (value_mul(&value1, &value2, &result)) {
            return SR_BO_MUL_GEN_ERR;
        }
        break;

    case BO_ADD:
        if (value_add(&value1, &value2, &result)) {
            return SR_BO_ADD_GEN_ERR;
        }
        break;

    case BO_DIV:
        switch (value_div(&value1, &value2, &result)) {
        case BO_OK:
            break;
        case BO_DIVIDE_BY_ZERO:
            return SR_BO_DIV_DBZ;
        default:
            return SR_BO_DIV_GEN_ERR;
        }
        break;

    case BO_SUB:
        if (value_sub(&value1, &value2, &result)) {
            return SR_BO_SUB_GEN_ERR;
        }
        break;

    case BO_REM:
        if (value_rem(&value1, &value2, &result)) {
            return SR_BO_REM_GEN_ERR;
        }
        break;
    default:
        return SR_BO_UNKNOWN;
    }

    stack_push(frame->stack, result);
    frame->pc++;
    return SR_OK;
}

static StepResult handle_get(VMContext* vm_context, IrInstruction* ir_instruction)
{
    GetOP* get = &ir_instruction->data.get;
    if (!get) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value;
    value.type = TYPE_BOOLEAN;
    value.data.bool_value = false;

    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_return(VMContext* vm_context, IrInstruction* ir_instruction)
{
    Frame* frame = vm_context->frame;
    CallStack* call_stack = vm_context->call_stack;

    Value value;
    stack_pop(frame->stack, &value);

    call_stack_pop(call_stack);

    if (call_stack->count > 0) {
        Frame* frame = call_stack_peek(call_stack);
        if (!frame) {
            // todo..
        }

        stack_push(frame->stack, value);
    }

    return SR_OK;
}

static bool handle_ift_aux(IfCondition condition, int value1, int value2)
{
    switch (condition) {
    case IF_EQ:
        return value1 == value2;
    case IF_NE:
        return value1 != value2;
    case IF_GT:
        return value1 > value2;
    case IF_LT:
        return value1 < value2;
    case IF_GE:
        return value1 >= value2;
    case IF_LE:
        return value1 <= value2;
    default:
        return false;
    }

    return false;
}

static StepResult handle_ifz(VMContext* vm_context, IrInstruction* ir_instruction)
{
    IfOP* ift = &ir_instruction->data.ift;
    if (!ift) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value;
    stack_pop(frame->stack, &value);

    bool result;

    if (value.type == TYPE_INT) {
        result = handle_ift_aux(ift->condition, value.data.int_value, 0);
    } else if (value.type == TYPE_CHAR) {
        result = handle_ift_aux(ift->condition, value.data.char_value, 0);
    } else if (value.type == TYPE_BOOLEAN) {
        result = handle_ift_aux(ift->condition, value.data.bool_value, 0);
    } else {
        return SR_INVALID_TYPE;
    }

    if (result) {
        frame->pc = ift->target;
    } else {
        frame->pc++;
    }

    return SR_OK;
}

static StepResult handle_ift(VMContext* vm_context, IrInstruction* ir_instruction)
{
    IfOP* ift = &ir_instruction->data.ift;
    if (!ift) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value1, value2;
    stack_pop(frame->stack, &value2);
    stack_pop(frame->stack, &value1);

    int a, b;
    if (value1.type == TYPE_INT) {
        a = value1.data.int_value;
    } else if (value1.type == TYPE_BOOLEAN) {
        a = (int)value1.data.bool_value;
    } else if (value1.type == TYPE_CHAR) {
        a = (int)value1.data.char_value;
    } else {
        return SR_INVALID_TYPE;
    }

    if (value2.type == TYPE_INT) {
        b = value2.data.int_value;
    } else if (value2.type == TYPE_BOOLEAN) {
        b = (int)value2.data.bool_value;
    } else if (value2.type == TYPE_CHAR) {
        b = (int)value2.data.char_value;
    } else {
        return SR_INVALID_TYPE;
    }

    bool result = handle_ift_aux(ift->condition, a, b);

    if (result) {
        frame->pc = ift->target;
    } else {
        frame->pc++;
    }

    return SR_OK;
}

static StepResult handle_store(VMContext* vm_context, IrInstruction* ir_instruction)
{
    StoreOP* store = &ir_instruction->data.store;
    if (!store) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value1;
    stack_pop(frame->stack, &value1);

    int index = store->index;
    if (index >= frame->locals_count) {
        frame->locals_count = index + 1;
        frame->locals = realloc(frame->locals, frame->locals_count * sizeof(Value));
    }

    frame->locals[index] = value1;

    frame->pc++;
    return SR_OK;
}

static StepResult handle_goto(VMContext* vm_context, IrInstruction* ir_instruction)
{
    GotoOP* go2 = &ir_instruction->data.go2;
    if (!go2) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    frame->pc = go2->target;
    return SR_OK;
}

static StepResult handle_dup(VMContext* vm_context, IrInstruction* ir_instruction)
{
    Frame* frame = vm_context->frame;
    Value* value = stack_peek(frame->stack);
    if (!value) {
        // todo: handle this case
        // return SR_EMPTY_STACK;
    } else {
        stack_push(frame->stack, *value);
    }

    frame->pc++;
    return SR_OK;
}

// todo: handle array
static char* get_method_signature(InvokeOP* invoke)
{
    char* res = NULL;
    char* ref_name = strdup(invoke->ref_name);
    replace_char(ref_name, '/', '.');

    int capacity = 10;
    int args_len = 0;
    char* args = malloc(sizeof(char) * capacity);

    for (int i = 0; i < invoke->args_len; i++) {
        TypeKind tk = invoke->args[i]->kind;
        if (capacity <= args_len) {
            capacity *= 2;
            args = realloc(args, sizeof(char) * capacity);
            if (!args) {
                res = NULL;
                goto cleanup;
            }
        }

        switch (tk) {
        case TK_INT:
            args[args_len] = 'I';
            break;
        case TK_BOOLEAN:
            args[args_len] = 'Z';
            break;
        default:
            LOG_ERROR("Unable to handle type: %d, in get_method_signature", tk);
            return NULL;
        }

        args_len++;
    }

    args = realloc(args, sizeof(char) * args_len + 1);
    if (!args) {
        res = NULL;
        goto cleanup;
    }
    args[args_len] = '\0';

    char return_type[3];
    switch (invoke->return_type->kind) {
    case TK_INT:
        strcpy(return_type, "I");
        break;
    case TK_BOOLEAN:
        strcpy(return_type, "Z");
        break;
    case TK_VOID:
        strcpy(return_type, "V");
        break;
    case TK_ARRAY:
        if (invoke->return_type == make_array_type(TYPE_INT)) {
            strcpy(return_type, "[I");
        } else if (invoke->return_type == make_array_type(TYPE_BOOLEAN)) {
            strcpy(return_type, "[Z");
        } else {
            LOG_ERROR("Unable to handle array return type in get_method_signature");
            return NULL;
        }

        break;
    default:
        LOG_ERROR("Unable to handle return type: %d, in get_method_signature", invoke->return_type->kind);
        return NULL;
    }

    asprintf(&res, "%s.%s:(%s)%s", ref_name, invoke->method_name, args, return_type);

cleanup:
    free(args);
    free(ref_name);

    return res;
}

static StepResult handle_invoke(VMContext* vm_context, IrInstruction* ir_instruction)
{
    StepResult result = SR_OK;

    InvokeOP* invoke = &ir_instruction->data.invoke;
    if (!invoke) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;

    CallStack* call_stack = vm_context->call_stack;
    const Config* cfg = vm_context->cfg;

    char* method_id = get_method_signature(invoke);
    if (!method_id) {
        result = SR_INTERNAL_NULL_ERR;
        goto cleanup;
    }

    Method* m = method_create(method_id);
    if (!m) {
        result = SR_INTERNAL_NULL_ERR;
        goto cleanup;
    }

    char* root = strtok(method_id, ".");

    if (strcmp(root, "jpamb") == 0) {
        int locals_count;
        Value* locals = build_locals_from_frame(m, frame, &locals_count);
        Frame* new_frame = build_frame(m, cfg, locals, locals_count);

        call_stack_push(call_stack, new_frame);
    }

cleanup:
    method_delete(m);
    free(method_id);

    frame->pc++;

    return result;
}

// todo: handle multi dimensional arrays
static StepResult handle_array_length(VMContext* vm_context, IrInstruction* ir_instruction)
{
    ArrayLengthOP* array_length = &ir_instruction->data.array_length;
    if (!array_length) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value;
    if (stack_pop(frame->stack, &value)) {
        return SR_EMPTY_STACK;
    }

    if (value.type != TYPE_REFERENCE) {
        return SR_INVALID_TYPE;
    }

    ObjectValue* array = heap_get(vm_context->heap, value.data.ref_value);
    if (!array) {
        return SR_NULL_POINTER;
    }

    if (array->type->kind != TK_ARRAY) {
        return SR_INVALID_TYPE;
    }

    Value to_push = { .type = TYPE_INT };
    to_push.data.int_value = array->array.elements_count;

    stack_push(frame->stack, to_push);

    frame->pc++;
    return SR_OK;
}

static StepResult handle_new_array(VMContext* vm_context, IrInstruction* ir_instruction)
{
    NewArrayOP* new_array = &ir_instruction->data.new_array;
    if (!new_array) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value size;
    if (stack_pop(frame->stack, &size)) {
        return SR_EMPTY_STACK;
    }

    if (size.type != TYPE_INT) {
        return SR_INVALID_TYPE;
    }

    if (new_array->type == TYPE_INT) {
        ObjectValue* array = malloc(sizeof(ObjectValue));
        array->type = make_array_type(TYPE_INT);
        array->array.elements = malloc(sizeof(Value) * size.data.int_value);
        array->array.elements_count = size.data.int_value;

        Value ref = { .type = TYPE_REFERENCE };
        if (heap_insert(vm_context->heap, array, &ref.data.ref_value)) {
            // todo: heap should be dynamic
            return SR_OUT_OF_BOUNDS;
        }

        stack_push(frame->stack, ref);
    } else {
        return SR_INVALID_TYPE;
    }

    frame->pc++;

    return SR_OK;
}

static StepResult handle_array_store(VMContext* vm_context, IrInstruction* ir_instruction)
{
    ArrayStoreOP* array_store = &ir_instruction->data.array_store;
    if (!array_store) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;

    Value index;
    Value value;
    Value ref;

    if (stack_pop(frame->stack, &value)) {
        return SR_EMPTY_STACK;
    }

    if (stack_pop(frame->stack, &index)) {
        return SR_EMPTY_STACK;
    }

    if (stack_pop(frame->stack, &ref)) {
        return SR_EMPTY_STACK;
    }

    if (index.type != TYPE_INT) {
        return SR_INVALID_TYPE;
    }

    ObjectValue* array = heap_get(vm_context->heap, ref.data.ref_value);
    if (!array) {
        return SR_NULL_POINTER;
    }

    if (array->type != make_array_type(array_store->type) || array->type != make_array_type(value.type)) {
        return SR_INVALID_TYPE;
    }

    LOG_DEBUG("ELEMS COUNT: %d, INDEX: %d, VALUE: %d", array->array.elements_count, index.data.int_value, value.data.int_value);

    if (array->array.elements_count <= index.data.int_value) {
        return SR_OUT_OF_BOUNDS;
    }

    array->array.elements[index.data.int_value] = value;
    frame->pc++;

    return SR_OK;
}

static StepResult handle_array_load(VMContext* vm_context, IrInstruction* ir_instruction)
{
    ArrayLoadOP* array_load = &ir_instruction->data.array_load;
    if (!array_load) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value index;
    Value ref;

    if (stack_pop(frame->stack, &index)) {
        return SR_EMPTY_STACK;
    }

    if (stack_pop(frame->stack, &ref)) {
        return SR_EMPTY_STACK;
    }

    if (index.type != TYPE_INT) {
        return SR_INVALID_TYPE;
    }

    ObjectValue* array = heap_get(vm_context->heap, ref.data.ref_value);
    if (!array) {
        return SR_NULL_POINTER;
    }

    if (array->type != make_array_type(array_load->type)) {
        return SR_INVALID_TYPE;
    }

    if (array->array.elements_count <= index.data.int_value) {
        return SR_OUT_OF_BOUNDS;
    }

    Value value = array->array.elements[index.data.int_value];
    stack_push(frame->stack, value);

    frame->pc++;

    return SR_OK;
}

static StepResult handle_incr(VMContext* vm_context, IrInstruction* ir_instruction)
{
    IncrOP* incr = &ir_instruction->data.incr;
    if (!incr) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    if (incr->index < 0 || incr->index >= frame->locals_count) {
        return SR_OUT_OF_BOUNDS;
    }

    Value* value = &(frame->locals[incr->index]);

    if (value->type == TYPE_INT) {
        value->data.int_value += incr->amount;
    } else {
        return SR_INVALID_TYPE;
    }

    frame->pc++;

    return SR_OK;
}

static StepResult handle_skip(VMContext* vm_context, IrInstruction* ir_instruction)
{
    Frame* frame = vm_context->frame;
    frame->pc++;

    return SR_OK;
}

static StepResult handle_throw(VMContext* vm_context, IrInstruction* ir_instruction)
{
    return SR_ASSERTION_ERR;
}

static IrInstruction* get_ir_instruction(VMContext* vm_context)
{
    Frame* frame = vm_context->frame;
    IrFunction* ir_function = frame->ir_function;
    if (!ir_function) {
        return NULL;
    }
    if (frame->pc < 0 || frame->pc >= vector_length(ir_function->ir_instructions)) {
        return NULL;
    }
    return vector_get(ir_function->ir_instructions, frame->pc);
}

static OpHandler opcode_table[OP_COUNT] = {
    [OP_LOAD] = handle_load,
    [OP_PUSH] = handle_push,
    [OP_BINARY] = handle_binary,
    [OP_GET] = handle_get,
    [OP_RETURN] = handle_return,
    [OP_IF_ZERO] = handle_ifz,
    [OP_IF] = handle_ift,
    [OP_DUP] = handle_dup,
    [OP_INVOKE] = handle_invoke,
    [OP_NEW] = handle_skip,
    [OP_CAST] = handle_skip,
    [OP_THROW] = handle_throw,
    [OP_STORE] = handle_store,
    [OP_GOTO] = handle_goto,
    [OP_NEW_ARRAY] = handle_new_array,
    [OP_ARRAY_LENGTH] = handle_array_length,
    [OP_ARRAY_STORE] = handle_array_store,
    [OP_ARRAY_LOAD] = handle_array_load,
    [OP_INCR] = handle_incr,
};

static StepResult step(VMContext* vm_context)
{
    if (!vm_context->cfg) {
        return SR_INTERNAL_NULL_ERR;
    }

    if (!vm_context->call_stack) {
        return SR_EMPTY_STACK;
    }

    vm_context->frame = call_stack_peek(vm_context->call_stack);
    if (!vm_context->frame) {
        return SR_EMPTY_STACK;
    }

    IrInstruction* ir_instruction = get_ir_instruction(vm_context);
    if (!ir_instruction) {
        return SR_NULL_INSTRUCTION;
    }
    // LOG_DEBUG("Interpreting: %s", opcode_print(ir_instruction->opcode));

    Opcode opcode = ir_instruction->opcode;
    if (opcode < 0 || opcode >= OP_COUNT) {
        return SR_UNKNOWN_OPCODE;
    }

    return opcode_table[ir_instruction->opcode](vm_context, ir_instruction);
}

VMContext* interpreter_setup(const Method* m, const Options* opts, const Config* cfg)
{
    if (!m || !opts || !cfg) {
        return NULL;
    }

    Frame* frame = NULL;
    CallStack* call_stack = NULL;

    Heap* heap = heap_create();
    if (!heap) {
        goto cleanup;
    }

    const char* raw_params = opts->parameters ? opts->parameters : "";
    char* parameters = strdup(raw_params);
    LOG_DEBUG("BUILDING FRAME...");
    LOG_DEBUG("PARAMETERS: %s", parameters);

    int locals_count = 0;
    Value* locals = build_locals_from_str(heap, m, parameters, &locals_count);
    frame = build_frame(m, cfg, locals, locals_count);

    if (!frame) {
        goto cleanup;
    }

    call_stack = calloc(1, sizeof(CallStack));
    if (!call_stack) {
        goto cleanup;
    }

    call_stack->top = NULL;
    call_stack->count = 0;
    call_stack_push(call_stack, frame);

    VMContext* vm_context = malloc(sizeof(VMContext));
    vm_context->call_stack = call_stack;
    vm_context->frame = frame;
    vm_context->cfg = cfg;
    vm_context->heap = heap_create();

    return vm_context;

cleanup:
    // todo: free objects
    return NULL;
}

RuntimeResult interpreter_run(VMContext* vm_context)
{
    RuntimeResult result = RT_OK;

    CallStack* call_stack = vm_context->call_stack;
    if (!call_stack) {
        LOG_ERROR("CallStack is null");
        goto cleanup;
    }

    int i = 0;
    for (; i < ITERATION; i++) {
        if (call_stack->count > 0) {
            StepResult step_result = step(vm_context);
            switch (step_result) {
            case SR_OK:
                continue;
            case SR_BO_DIV_DBZ:
                result = RT_DIVIDE_BY_ZERO;
                break;
            case SR_ASSERTION_ERR:
                result = RT_ASSERTION_ERR;
                break;
            case SR_OUT_OF_BOUNDS:
                result = RT_OUT_OF_BOUNDS;
                break;
            case SR_NULL_POINTER:
                result = RT_NULL_POINTER;
                break;
            default:
                result = RT_UNKNOWN_ERROR;
                break;
            }

            // LOG_ERROR("%s", step_result_signature[step_result]);

            goto cleanup;
        } else {
            result = RT_OK;
            goto cleanup;
        }
    }

cleanup:
    if (result) {
        return result;
    }

    if (i == ITERATION) {
        return RT_INFINITE;
    }

    // todo free memory
    return result;
}
