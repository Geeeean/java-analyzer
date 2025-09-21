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

typedef struct {
    Frame* frames;
    int count;
    int capacity;
} CallStack;

static char arg_types_signature[] = {
    [TYPE_INT] = 'I',
    [TYPE_CHAR] = 'C',
    [TYPE_ARRAY] = '[',
};

static ValueType get_type(char c)
{
    if (c == arg_types_signature[TYPE_INT]) {
        return TYPE_INT;
    } else if (c == arg_types_signature[TYPE_CHAR]) {
        return TYPE_CHAR;
    } else if (c == arg_types_signature[TYPE_ARRAY]) {
        return TYPE_ARRAY;
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

    char* copy = strdup(token + 2);
    token = strtok(NULL, "[:_;]");

    int array_size = 0;

    while (token != NULL) {
        array_size += 1;
        token = strtok(NULL, "[:_;]");
    }

    Value* array = malloc(array_size * sizeof(Value) + 1);
    int i = 0;

    token = strtok(copy, "[:_;]");
    while (token != NULL) {
        if (type == TYPE_INT) {
            array[i].type = TYPE_INT;
            array[i].data.int_value = strtol(token, NULL, 10);
        }
        // todo:
        /* else if (other types) {...} */
        else {
        }
        token = strtok(NULL, "[:_;]");
        i++;
    }
    free(copy);

    array[array_size].type = TYPE_VOID;

    return array;
}

static Value* parse_next_parameter(char** arguments, char* token)
{
    if (arguments == NULL || *arguments == NULL || token == NULL) {
        return NULL;
    }

    ValueType type = get_type(**arguments);
    if (type < 0) {
        fprintf(stderr, "Unknown arg type in method signature: %c\n", **arguments);
        return NULL;
    }

    (*arguments) += 1;

    Value* value = malloc(sizeof(Value));
    value->type = type;

    if (type == TYPE_ARRAY) {
        ValueType array_type = get_type(**arguments);
        (*arguments) += 1;
        value->data.array_value = parse_array(array_type, token);
    } else if (type == TYPE_INT) {
        value->type = type;
        value->data.int_value = (int)strtol(token, NULL, 10);
    } else {
        fprintf(stderr, "Not handled type in method signature: %c\n", **arguments);
        return NULL;
    }

    return value;
}

int interpreter_execute(InstructionTable* instruction_table, const Method* m, char* parameters)
{
    if (!instruction_table || !m || !parameters) {
        return 1;
    }

    parameter_fix(parameters);
    char* arguments = method_get_arguments(m);

    printf("ARGUMENTS  %s\n", arguments);
    printf("PARAMETERS %s\n\n", parameters);

    char* strtok_state = NULL;

    char* token = strtok_r(parameters, "(), ", &strtok_state);
    while (token != NULL) {
        printf("token: %s, value: ", token);
        Value* value = parse_next_parameter(&arguments, token);
        value_print(value);
        printf("\n");
        token = strtok_r(NULL, "), ", &strtok_state);
    }

    return 0;
}
