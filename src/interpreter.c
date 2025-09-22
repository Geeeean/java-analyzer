#include "interpreter.h"
#include "stack.h"
#include "string.h"

#include <stdlib.h>

typedef struct {
    Value* locals;
    int locals_count;
    Stack* stack;
    int pc;
} Frame;

static void frame_print(Frame* frame)
{
    printf("PC: %d\n", frame->pc);
    printf("LOCALS:\n");
    for (int i = 0; i < frame->locals_count; i++) {
        printf("[%2d]: ", i);
        value_print(frame->locals + i);
        printf("\n");
    }
}
typedef struct CallStackNode {
    Frame* frame;
    struct CallStackNode* next;
} CallStackNode;

typedef struct {
    CallStackNode* top;
    int count;
} CallStack;

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

static char arg_types_signature[] = {
    [TYPE_INT] = 'I',
    [TYPE_CHAR] = 'C',
    [TYPE_ARRAY] = '[',
    [TYPE_BOOLEAN] = 'Z',
};

static ValueType get_type(char c)
{
    if (c == arg_types_signature[TYPE_INT]) {
        return TYPE_INT;
    } else if (c == arg_types_signature[TYPE_CHAR]) {
        return TYPE_CHAR;
    } else if (c == arg_types_signature[TYPE_ARRAY]) {
        return TYPE_ARRAY;
    } else if (c == arg_types_signature[TYPE_BOOLEAN]) {
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
static Value* parse_array(ValueType type, char* to_parse_array)
{
    if (type < 0 /* || todo */ || !to_parse_array) {
        return NULL;
    }

    char* token = strtok(to_parse_array, "[:_;]");
    if (!token) {
        return NULL;
    }

    if (type != get_type(*token)) {
        return NULL;
    }

    int count = 0;
    int capacity = 10;

    Value* array = malloc(capacity * sizeof(Value));

    while (token != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            array = realloc(array, capacity * sizeof(Value));
        }

        if (type == TYPE_INT) {
            array[count].type = TYPE_INT;
            array[count].data.int_value = strtol(token, NULL, 10);
        }
        // todo:
        /* else if (other types) {...} */
        else {
            goto cleanup;
        }

        token = strtok(NULL, "[:_;]");
        count++;
    }

    array = realloc(array, count * sizeof(Value) + 1);
    array[count].type = TYPE_VOID;
    return array;

cleanup:
    free(array);
    return NULL;
}

static int parse_next_parameter(char** arguments, char* token, Value* value)
{
    if (!arguments || !(*arguments) || !token || !value) {
        return 1;
    }

    ValueType type = get_type(**arguments);
    if (type < 0) {
        fprintf(stderr, "Unknown arg type in method signature: %c\n", **arguments);
        return 2;
    }

    (*arguments)++;

    value->type = type;

    if (type == TYPE_ARRAY) {
        ValueType array_type = get_type(**arguments);
        (*arguments)++;
        value->data.array_value = parse_array(array_type, token);
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

static Frame* build_first_frame(char* arguments, char* parameters)
{
    Frame* frame = malloc(sizeof(Frame));
    if (!frame) {
        goto cleanup;
    }

    frame->pc = 0;
    frame->stack = stack_new();
    frame->locals_count = 0;

    int capacity = 10;
    frame->locals = malloc(capacity * sizeof(Value));

    parameter_fix(parameters);
    char* strtok_state = NULL;
    char* token = strtok_r(parameters, "(), ", &strtok_state);
    while (token != NULL) {
        if (frame->locals_count >= capacity) {
            capacity *= 2;
            frame->locals = realloc(frame->locals, capacity * sizeof(Value));
            if (!frame->locals) {
                // MEMORY LEAK
                // todo cleanup locals in case of error
                frame = NULL;
                goto cleanup;
            }
        }

        Value value;
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

    frame->locals = realloc(frame->locals, frame->locals_count * sizeof(Value));
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

static int step(CallStack* call_stack, InstructionTable* instruction_table)
{
    Frame* frame = call_stack_peek(call_stack);
    if (!frame) {
        return 1;
    }

    Instruction* instruction = instruction_table->instructions[frame->pc];

    // printf("Interpreting: %s\n", opcode_print(instruction->opcode));

    switch (instruction->opcode) {
    case OP_LOAD: {
        Value value = frame->locals[instruction->data.load.index];
        stack_push(frame->stack, &value);
    };
        frame->pc++;
        break;

    case OP_PUSH: {
        Value value = instruction->data.push.value;
        stack_push(frame->stack, &value);
    };
        frame->pc++;
        break;

    case OP_BINARY: {
        Value value1, value2, result;
        if (stack_pop(frame->stack, &value2)) {
            fprintf(stderr, "Cant pop from stack\n");
            return 2;
        }

        if (stack_pop(frame->stack, &value1)) {
            fprintf(stderr, "Cant pop from stack\n");
            return 2;
        }

        switch (instruction->data.binary.op) {
        case BO_MUL:
            if (value_mul(&value1, &value2, &result)) {
                fprintf(stderr, "Error when handling MUL op\n");
                return 3;
            }

            stack_push(frame->stack, &result);
            break;
        case BO_ADD:
            break;
        case BO_DIV: {
            int div_res = value_div(&value1, &value2, &result);
            if (div_res) {
                if (div_res == 3) {
                    printf("divide by zero\n");
                } else {
                    fprintf(stderr, "Error when handling DIV op\n");
                }
                return 5;
            }
            stack_push(frame->stack, &result);
        };
            break;
        case BO_SUB:
            if (value_sub(&value1, &value2, &result)) {
                fprintf(stderr, "Error when handling SUB op\n");
                return 3;
            }

            stack_push(frame->stack, &result);
            break;
        default:
            fprintf(stderr, "Unknown binary operation\n");
            return 7;
        }
    };
        frame->pc++;
        break;
    case OP_GET: {
        Value value;
        value.type = TYPE_BOOLEAN;
        value.data.bool_value = false;

        stack_push(frame->stack, &value);
    };
        frame->pc++;
        break;
    case OP_RETURN: {
        Value value;
        stack_pop(frame->stack, &value);
        // printf("Returning: ");
        // value_print(&value);
    };
        call_stack_pop(call_stack);
        break;
    case OP_IF_ZERO: {
        Value value;
        stack_pop(frame->stack, &value);

        switch (value.type) {
        case TYPE_INT:
            switch (instruction->data.ifz.condition) {
            case IFZ_EQ:
                if (value.data.int_value == 0) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            case IFZ_NE:
                if (value.data.int_value != 0) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            case IFZ_GT:
                if (value.data.int_value > 0) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            case IFZ_LT:
                if (value.data.int_value < 0) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            case IFZ_GE:
                if (value.data.int_value >= 0) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            case IFZ_LE:
                if (value.data.int_value <= 0) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            default:
                fprintf(stderr, "Dont know how to handle ifz: %d\n", instruction->data.ifz.condition);
                return 9;
            }
            break;
        case TYPE_BOOLEAN:

            switch (instruction->data.ifz.condition) {
            case IFZ_EQ:
                if (!value.data.bool_value) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            case IFZ_NE:
                if (value.data.bool_value) {
                    frame->pc = instruction->data.ifz.target;
                } else {
                    frame->pc++;
                }
                break;
            default:
                fprintf(stderr, "Cant handle this ifz op on boolean\n");
                return 10;
            }
            break;
        default:
            fprintf(stderr, "Dont know how to ifz onto type %d\n", value.type);
            return 8;
        }
    };
        break;
    case OP_NEW:
    case OP_DUP:
    case OP_INVOKE:
    case OP_THROW:
        printf("assertion error\n");
        return 11;
        break;
    case OP_COUNT:
        break;
    default:
        fprintf(stderr, "Error while matching instruction during execution\n");
        return 3;
    }

    return 0;
}

int interpreter_execute(InstructionTable* instruction_table, const Method* m, const char* parameters)
{
    if (!instruction_table || !m || !parameters) {
        return 1;
    }

    char* arguments = strdup(method_get_arguments(m));
    char* params = strdup(parameters);

    Frame* frame = build_first_frame(arguments, params);
    if (!frame) {
        return 2;
    }

    free(arguments);
    free(params);

    // frame_print(frame);

    CallStack* call_stack = malloc(sizeof(CallStack));
    call_stack_push(call_stack, frame);

    for (int i = 0; i < 1000; i++) {
        if (call_stack->count > 0) {
            if (step(call_stack, instruction_table)) {
                return 1;
            }
        } else {
            printf("ok\n");
            break;
        }
    }

    return 0;
}
