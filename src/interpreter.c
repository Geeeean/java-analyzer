// todo handle pop error
#include "interpreter.h"
#include "stack.h"
#include "string.h"

#include <stdlib.h>

#define ITERATION 1000

// todo: reason about splitting in specific code for each op
typedef enum {
    SR_OK,
    SR_OUT_OF_BOUNDS,
    SR_NULL_INSTRUCTION,
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
    PrimitiveType* locals;
    int locals_count;
    Stack* stack;
    int pc;
} Frame;

typedef struct CallStackNode {
    Frame* frame;
    struct CallStackNode* next;
} CallStackNode;

typedef struct {
    CallStackNode* top;
    int count;
} CallStack;

typedef struct {
    CallStack* call_stack;
} Interpreter;

static void call_stack_push(CallStack* call_stack, Frame* frame)
{
    if (!call_stack || !frame)
        return;

    CallStackNode* new_node = malloc(sizeof(CallStackNode));
    if (!new_node) {
        fprintf(stderr, "CallStack push: malloc failed\n");
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

static Type get_type(char c)
{
    if (c == args_type_signature[TYPE_INT]) {
        return TYPE_INT;
    } else if (c == args_type_signature[TYPE_CHAR]) {
        return TYPE_CHAR;
    } else if (c == args_type_signature[TYPE_ARRAY]) {
        return TYPE_ARRAY;
    } else if (c == args_type_signature[TYPE_BOOLEAN]) {
        return TYPE_BOOLEAN;
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

// todo: embed count inside array structure
static PrimitiveType* parse_array(Type type, char* to_parse_array)
{
    return NULL;
    //    if (type < 0 /* || todo */ || !to_parse_array) {
    //        return NULL;
    //    }
    //
    //    char* token = strtok(to_parse_array, "[:_;]");
    //    if (!token) {
    //        return NULL;
    //    }
    //
    //    if (type != get_type(*token)) {
    //        return NULL;
    //    }
    //
    //    int count = 0;
    //    int capacity = 10;
    //
    //    Value* array = malloc(capacity * sizeof(Value));
    //
    //    while (token != NULL) {
    //        if (count >= capacity) {
    //            capacity *= 2;
    //            array = realloc(array, capacity * sizeof(Value));
    //        }
    //
    //        if (type == TYPE_INT) {
    //            array[count].type = TYPE_INT;
    //            array[count].data.int_value = strtol(token, NULL, 10);
    //        }
    //        // todo:
    //        /* else if (other types) {...} */
    //        else {
    //            goto cleanup;
    //        }
    //
    //        token = strtok(NULL, "[:_;]");
    //        count++;
    //    }
    //
    //    array = realloc(array, count * sizeof(Value) + 1);
    //    array[count].type = TYPE_VOID;
    //    return array;
    //
    // cleanup:
    //    free(array);
    //    return NULL;
}

static int parse_next_parameter(char** arguments, char* token, PrimitiveType* value)
{
    if (!arguments || !(*arguments) || !token || !value) {
        return 1;
    }

    Type type = get_type(**arguments);
    if (type < 0) {
        fprintf(stderr, "Unknown arg type in method signature: %c\n", **arguments);
        return 2;
    }

    (*arguments)++;

    value->type = type;

    if (type == TYPE_ARRAY) {
        Type array_type = get_type(**arguments);
        (*arguments)++;
        // todo
        // value->data.array_value = parse_array(array_type, token);
    } else if (type == TYPE_INT) {
        value->data.int_value = (int)strtol(token, NULL, 10);
    } else if (type == TYPE_BOOLEAN) {
        if (strcmp(token, "false") == 0) {
            value->data.bool_value = false;
        } else if (strcmp(token, "true") == 0) {
            value->data.bool_value = true;
        } else {
            fprintf(stderr, "Type is bool but token is neither 'true' or 'false': %s\n", token);
            return 4;
        }
    } else {
        fprintf(stderr, "Not handled type in method signature: %c\n", **arguments);
        return 3;
    }

    return 0;
}

static Frame* build_frame(char* arguments, char* parameters)
{
    Frame* frame = malloc(sizeof(Frame));
    if (!frame) {
        goto cleanup;
    }

    frame->pc = 0;
    frame->stack = stack_new();
    frame->locals_count = 0;

    int capacity = 10;
    frame->locals = malloc(capacity * sizeof(PrimitiveType));

    parameter_fix(parameters);
    char* strtok_state = NULL;
    char* token = strtok_r(parameters, "(), ", &strtok_state);
    while (token != NULL) {
        if (frame->locals_count >= capacity) {
            capacity *= 2;
            frame->locals = realloc(frame->locals, capacity * sizeof(PrimitiveType));
            if (!frame->locals) {
                // MEMORY LEAK
                // todo cleanup locals in case of error
                frame = NULL;
                goto cleanup;
            }
        }

        PrimitiveType value;
        if (parse_next_parameter(&arguments, token, &value)) {
            // MEMORY LEAK
            // todo cleanup locals in case of error
            frame = NULL;
            goto cleanup;
        }

        // todo check for int, ref, float
        frame->locals[frame->locals_count] = value;

        token = strtok_r(NULL, "), ", &strtok_state);
        frame->locals_count++;
    }

    frame->locals = realloc(frame->locals, frame->locals_count * sizeof(PrimitiveType));
    if (!frame->locals) {
        // MEMORY LEAK
        // todo cleanup locals in case of error
        frame = NULL;
        goto cleanup;
    }

    return frame;

cleanup:
    return NULL;
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

    PrimitiveType value = frame->locals[index];
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_push(Frame* frame, PushOP* push)
{
    if (!push) {
        return SR_NULL_INSTRUCTION;
    }

    PrimitiveType value = push->value;
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_binary(Frame* frame, BinaryOP* binary)
{
    PrimitiveType value1, value2, result;
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

    PrimitiveType value;
    value.type = TYPE_BOOLEAN;
    value.data.bool_value = false;

    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

// todo push into the stack of the next frame
static StepResult handle_return(Frame* frame, CallStack* call_stack)
{
    PrimitiveType value;
    stack_pop(frame->stack, &value);
    call_stack_pop(call_stack);

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
    PrimitiveType value;
    stack_pop(frame->stack, &value);

    bool result;

    switch (value.type) {
    case TYPE_INT:
        result = handle_ift_aux(ift->condition, value.data.int_value, 0);
        break;
    case TYPE_CHAR:
        result = handle_ift_aux(ift->condition, value.data.char_value, 0);
        break;
    case TYPE_BOOLEAN:
        result = handle_ift_aux(ift->condition, value.data.bool_value, 0);
        break;
    default:
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
    PrimitiveType value1, value2;
    stack_pop(frame->stack, &value2);
    stack_pop(frame->stack, &value1);

    if (value1.type != value2.type) {
        return SR_INVALID_TYPE;
    }

    bool result;

    switch (value1.type) {
    case TYPE_INT:
        result = handle_ift_aux(ift->condition, value1.data.int_value, value2.data.int_value);
        break;
    case TYPE_CHAR:
        result = handle_ift_aux(ift->condition, value1.data.char_value, value2.data.char_value);
        break;
    case TYPE_BOOLEAN:
        result = handle_ift_aux(ift->condition, value1.data.bool_value, value2.data.bool_value);
        break;
    default:
        return SR_INVALID_TYPE;
    }

    if (result) {
        frame->pc = ift->target;
    } else {
        frame->pc++;
    }

    return SR_OK;
}

static StepResult handle_store(Frame* frame, StoreOP* store)
{
    PrimitiveType value1;
    stack_pop(frame->stack, &value1);

    int index = store->index;
    if (index >= frame->locals_count) {
        frame->locals_count = index + 1;
        frame->locals = realloc(frame->locals, frame->locals_count * sizeof(PrimitiveType));
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
    PrimitiveType* value = stack_peek(frame->stack);
    
    //todo: handle this case
    if (value) {
        stack_push(frame->stack, *value);
    }

    frame->pc++;
    return SR_OK;
}

static StepResult handle_new_array(Frame* frame, NewArrayOP* new_array)
{

    return SR_OK;
}

static StepResult step(CallStack* call_stack, InstructionTable* instruction_table)
{
    Frame* frame = call_stack_peek(call_stack);
    if (!frame) {
        return 1;
    }

    Instruction* instruction = instruction_table->instructions[frame->pc];

    fprintf(stderr, "Interpreting: %s\n", opcode_print(instruction->opcode));
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
    case OP_CAST:
    case OP_NEW:
    case OP_INVOKE:
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

    case OP_COUNT:
        break;

    default:
        result = SR_UNKNOWN_OPCODE;
        break;
    }

    return result;
}

RuntimeResult interpreter_run(InstructionTable* instruction_table, const Method* m, const char* parameters)
{
    RuntimeResult result = RT_OK;

    if (!instruction_table || !m || !parameters) {
        result = RT_NULL_PARAMETERS;
        goto cleanup;
    }

    char* arguments = strdup(method_get_arguments(m));
    char* params = strdup(parameters);

    Frame* frame = build_frame(arguments, params);
    if (!frame) {
        result = RT_CANT_BUILD_FRAME;
        goto cleanup;
    }

    CallStack* call_stack = malloc(sizeof(CallStack));
    call_stack_push(call_stack, frame);

    int i = 0;
    for (; i < ITERATION; i++) {
        if (call_stack->count > 0) {
            StepResult step_result = step(call_stack, instruction_table);
            switch (step_result) {
            case SR_OK:
                continue;
            case SR_BO_DIV_DBZ:
                result = RT_DIVIDE_BY_ZERO;
                break;
            case SR_ASSERTION_ERR:
                result = RT_ASSERTION_ERR;
                break;
            default:
                result = RT_UNKNOWN_ERROR;
                break;
            }

            fprintf(stderr, "Error: %s\n", step_result_signature[step_result]);

            goto cleanup;
        } else {
            result = RT_OK;
            goto cleanup;
        }
    }

cleanup:
    // todo: deallocate call_stack, frame, ...

    free(arguments);
    free(params);

    if (result) {
        return result;
    }

    if (i == ITERATION) {
        return RT_INFINITE;
    }

    return RT_OK;
}
