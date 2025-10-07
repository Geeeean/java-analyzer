// todo handle pop error
#include "interpreter.h"
#include "cli.h"
#include "heap.h"
#include "log.h"
#include "stack.h"
#include "string.h"
#include "utils.h"
#include "value.h"

#include <stdlib.h>

#define ITERATION 1000

// todo: use hashmap
typedef struct ITItem ITItem;
struct ITItem {
    char* method_id;
    InstructionTable* instruction_table;
    ITItem* next;
};

// to be freed
ITItem* it_map = NULL;

InstructionTable* get_instruction_table(const Method* m, const Config* cfg)
{
    for (ITItem* it = it_map; it != NULL; it = it->next) {
        if (strcmp(method_get_id(m), it->method_id) == 0) {
            return it->instruction_table;
        }
    }

    ITItem* it = malloc(sizeof(ITItem));
    it->instruction_table = instruction_table_build(m, cfg);
    it->next = it_map;
    it->method_id = strdup(method_get_id(m));

    return it->instruction_table;
}

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
    [SR_BO_DIV_GEN_ERR] = "SR_BO_DIV_GEN_ERR",
    [SR_BO_DIV_DBZ] = "SR_BO_DIV_DBZ",
    [SR_BO_ADD_GEN_ERR] = "SR_BO_ADD_GEN_ERR",
    [SR_BO_SUB_GEN_ERR] = "SR_BO_SUB_GEN_ERR",
    [SR_UNKNOWN_OPCODE] = "SR_UNKNOWN_OPCODE",
    [SR_ASSERTION_ERR] = "SR_ASSERTION_ERR",
    [SR_INVALID_TYPE] = "SR_INVALID_TYPE",
};

typedef struct {
    Value* locals;
    Stack* stack;
    InstructionTable* instruction_table;
    int locals_count;
    int pc;
} Frame;

typedef struct CallStackNode {
    Frame* frame;
    struct CallStackNode* next;
} CallStackNode;

struct CallStack {
    CallStackNode* top;
    int count;
};

typedef struct {
    CallStack* call_stack;
} Interpreter;

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
    if (!call_stack) {
        return NULL;
    }

    return call_stack->top->frame;
}

static TypeKind get_tk(char c)
{
    if (c == args_type_signature[TK_INT]) {
        return TK_INT;
    } else if (c == args_type_signature[TK_CHAR]) {
        return TK_CHAR;
    } else if (c == args_type_signature[TK_ARRAY]) {
        return TK_ARRAY;
    } else if (c == args_type_signature[TK_BOOLEAN]) {
        return TK_BOOLEAN;
    }

    return -1;
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
static int parse_array(char** arguments, char* token, ObjectValue* array)
{
    char* values = strtok(token, ":_;[]");
    int capacity = 10;
    int* len = &array->array.elements_count;
    array->array.elements = malloc(sizeof(Value) * capacity);

    TypeKind tk = get_tk(**arguments);
    if (tk == TK_INT) {
        array->type = make_array_type(TYPE_INT);

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
    } else if (tk == TK_CHAR) {
        array->type = make_array_type(TYPE_CHAR);

        while ((values = strtok(NULL, ";[]_'\"")) != NULL) {
            if (*len >= capacity) {
                capacity *= 2;
                array->array.elements = realloc(array->array.elements, sizeof(Value) * capacity);
            }

            // LOG_DEBUG("PARSING VALUE: %s", values);

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

static int parse_next_parameter(char** arguments, char* token, Value* value)
{
    if (!arguments || !(*arguments) || !token || !value) {
        return 1;
    }

    TypeKind tk = get_tk(**arguments);
    if (tk < 0) {
        LOG_ERROR("Unknown arg type in method signature: %c", **arguments);
        return 2;
    }

    (*arguments)++;

    if (tk == TK_ARRAY) {
        ObjectValue* array = malloc(sizeof(ObjectValue));

        if (parse_array(arguments, token, array)) {
            LOG_ERROR("While handling parse array");
            return 10;
        }

        value->type = TYPE_REFERENCE;

        if (heap_insert(array, &value->data.ref_value)) {
            LOG_ERROR("While inserting array into heap");
            return 11;
        }
    } else if (tk == TK_INT) {
        value->type = TYPE_INT;
        value->data.int_value = (int)strtol(token, NULL, 10);
    } else if (tk == TK_BOOLEAN) {
        value->type = TYPE_BOOLEAN;

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

static Value* build_locals_from_str(const Method* m, char* parameters, int* locals_count)
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
        if (parse_next_parameter(&arguments, token, &value)) {
            return NULL;
        }

        // todo check for int, ref, float
        locals[*locals_count] = value;

        token = strtok_r(NULL, "), ", &strtok_state);
        (*locals_count)++;
    }

    locals = realloc(locals, *locals_count * sizeof(Value));
    return locals;
}

// todo: make it better
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
    frame->instruction_table = get_instruction_table(m, cfg);

    return frame;
}

static StepResult handle_load(Frame* frame, LoadOP* load)
{
    if (!load) {
        return SR_NULL_INSTRUCTION;
    }

    int index = load->index;
    if (index >= frame->locals_count) {
        return SR_OUT_OF_BOUNDS;
    }

    Value value = frame->locals[index];
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_push(Frame* frame, PushOP* push)
{
    if (!push) {
        return SR_NULL_INSTRUCTION;
    }

    Value value = push->value;
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_binary(Frame* frame, BinaryOP* binary)
{
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

static StepResult handle_get(Frame* frame, GetOP* get)
{
    if (!get) {
        return SR_NULL_INSTRUCTION;
    }

    Value value;
    value.type = TYPE_BOOLEAN;
    value.data.bool_value = false;

    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

// todo push into the stack of the next frame
static StepResult handle_return(Frame* frame, CallStack* call_stack)
{
    Value value;
    stack_pop(frame->stack, &value);

    call_stack_pop(call_stack);

    if (call_stack->count > 0) {
        Frame* frame = call_stack_peek(call_stack);
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

static StepResult handle_ifz(Frame* frame, IfOP* ift)
{
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

static StepResult handle_ift(Frame* frame, IfOP* ift)
{
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

static StepResult handle_store(Frame* frame, StoreOP* store)
{
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

static StepResult handle_goto(Frame* frame, GotoOP* go2)
{
    frame->pc = go2->target;
    return SR_OK;
}

static StepResult handle_dup(Frame* frame, DupOP* dup)
{
    Value* value = stack_peek(frame->stack);

    // todo: handle this case
    if (value) {
        stack_push(frame->stack, *value);
    }

    frame->pc++;
    return SR_OK;
}

// todo: handle array
static char* get_method_signature(InvokeOP* invoke)
{
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

    char return_type;
    switch (invoke->return_type->kind) {
    case TK_INT:
        return_type = 'I';
        break;
    case TK_BOOLEAN:
        return_type = 'Z';
        break;
    case TK_VOID:
        return_type = 'V';
        break;
    default:
        LOG_ERROR("Unable to handle type: %d, in get_method_signature", invoke->return_type->kind);
        return NULL;
    }

    char* res;
    asprintf(&res, "%s.%s:(%s)%c", ref_name, invoke->method_name, args, return_type);

    free(args);
    free(ref_name);

    return res;
}

static StepResult handle_invoke(CallStack* call_stack, Frame* frame, const Config* cfg, InvokeOP* invoke)
{
    char* method_id = get_method_signature(invoke);
    Method* m = method_create(method_id);
    char* root = strtok(method_id, ".");
    LOG_DEBUG("INVOKE METHOD ROOT: %s", root);

    if (strcmp(root, "jpamb") == 0) {
        int locals_count;
        Value* locals = build_locals_from_frame(m, frame, &locals_count);
        Frame* new_frame = build_frame(m, cfg, locals, locals_count);

        call_stack_push(call_stack, new_frame);
    }

    free(method_id);

    frame->pc++;

    return SR_OK;
}

// todo: handle multi dimensional arrays
static StepResult handle_array_length(Frame* frame, ArrayLengthOP* array_length)
{
    Value value;
    if (stack_pop(frame->stack, &value)) {
        return SR_EMPTY_STACK;
    }

    if (value.type != TYPE_REFERENCE) {
        return SR_INVALID_TYPE;
    }

    ObjectValue* array = heap_get(value.data.ref_value);
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

static StepResult handle_new_array(Frame* frame, NewArrayOP* new_array)
{
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
        if (heap_insert(array, &ref.data.ref_value)) {
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

static StepResult handle_array_store(Frame* frame, ArrayStoreOP* array_store)
{
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

    ObjectValue* array = heap_get(ref.data.ref_value);
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

static StepResult handle_array_load(Frame* frame, ArrayLoadOP* array_load)
{

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

    ObjectValue* array = heap_get(ref.data.ref_value);
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

static StepResult handle_incr(Frame* frame, IncrOP* incr)
{
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
static StepResult step(CallStack* call_stack, const Config* cfg)
{
    Frame* frame = call_stack_peek(call_stack);
    if (!frame) {
        return 1;
    }

    InstructionTable* instruction_table = frame->instruction_table;

    Instruction* instruction = instruction_table->instructions[frame->pc];

    LOG_DEBUG("Interpreting: %s", opcode_print(instruction->opcode));
    StepResult result = SR_OK;

    switch (instruction->opcode) {
    case OP_LOAD:
        result = handle_load(frame, &instruction->data.load);
        break;

    case OP_PUSH:
        result = handle_push(frame, &instruction->data.push);
        break;

    case OP_BINARY:
        result = handle_binary(frame, &instruction->data.binary);
        break;

    case OP_GET:
        result = handle_get(frame, &instruction->data.get);
        break;

    case OP_RETURN:
        result = handle_return(frame, call_stack);
        break;

    case OP_IF_ZERO:
        result = handle_ifz(frame, &instruction->data.ift);
        break;

    case OP_IF:
        result = handle_ift(frame, &instruction->data.ift);
        break;

    case OP_DUP:
        result = handle_dup(frame, &instruction->data.dup);
        break;

    case OP_INVOKE:
        result = handle_invoke(call_stack, frame, cfg, &instruction->data.invoke);
        break;

    case OP_NEW:
    case OP_CAST:
        frame->pc++;
        break;

    case OP_THROW:
        result = SR_ASSERTION_ERR;
        break;

    case OP_STORE:
        result = handle_store(frame, &instruction->data.store);
        break;

    case OP_GOTO:
        result = handle_goto(frame, &instruction->data.go2);
        break;

    case OP_NEW_ARRAY:
        result = handle_new_array(frame, &instruction->data.new_array);
        break;

    case OP_ARRAY_LENGTH:
        result = handle_array_length(frame, &instruction->data.array_length);
        break;

    case OP_ARRAY_STORE:
        result = handle_array_store(frame, &instruction->data.array_store);
        break;

    case OP_ARRAY_LOAD:
        result = handle_array_load(frame, &instruction->data.array_load);
        break;

    case OP_INCR:
        result = handle_incr(frame, &instruction->data.incr);
        break;

    case OP_COUNT:
        break;

    default:
        result = SR_UNKNOWN_OPCODE;
        break;
    }

    return result;
}

CallStack* interpreter_setup(const Method* m, const Options* opts, const Config* cfg)
{
    if (!m || !opts || !cfg) {
        return NULL;
    }

    char* parameters = strdup(opts->parameters);

    LOG_DEBUG("BUILDING FRAME...");
    int locals_count;
    Value* locals = build_locals_from_str(m, parameters, &locals_count);
    Frame* frame = build_frame(m, cfg, locals, locals_count);
    LOG_DEBUG("LOCALS COUNT: %d", locals_count);

    if (!frame) {
        return NULL;
    }

    CallStack* call_stack = malloc(sizeof(CallStack));
    call_stack_push(call_stack, frame);

    return call_stack;
}

RuntimeResult interpreter_run(CallStack* call_stack, const Config* cfg)
{
    RuntimeResult result = RT_OK;

    if (!call_stack) {
        LOG_ERROR("CIAO");
    }

    int i = 0;
    for (; i < ITERATION; i++) {
        if (call_stack->count > 0) {
            StepResult step_result = step(call_stack, cfg);
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

            LOG_ERROR("%s", step_result_signature[step_result]);

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
