// todo handle pop error, use vector
#include "interpreter.h"
#include "cli.h"
#include "heap.h"
#include "log.h"
#include "opcode.h"
#include "stack.h"
#include "string.h"
#include "utils.h"
#include "value.h"
#include "coverage.h"

#include <stdlib.h>

#define ITERATION 100000

// todo: use hashmap
typedef struct ITItem ITItem;
struct ITItem {
    const Method* method;
    InstructionTable* instruction_table;
    ITItem* next;
};

// to be freed
ITItem* it_map = NULL;

InstructionTable* get_instruction_table(const Method* m, const Config* cfg)
{
    for (ITItem* it = it_map; it != NULL; it = it->next) {
     if (it->method == m) {
       return it->instruction_table;
     }
    }

    ITItem* it = malloc(sizeof(ITItem));
    it->method = m;
    it->instruction_table = instruction_table_build(m, cfg);
    it->next = it_map;
    it_map = it;

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
    InstructionTable* instruction_table;
    int locals_count;
    int pc;
} Frame;

typedef StepResult (*OpHandler)(VMContext*, Instruction*);

typedef struct CallStackNode {
    Frame* frame;
    struct CallStackNode* next;
} CallStackNode;

typedef struct {
    CallStackNode* top;
    int count;
} CallStack;

struct VMContext {
    CallStack* call_stack;
    const Config* cfg;
    Frame* frame;
    uint8_t* coverage_bitmap;
};

struct PersistentVMContext {
  CallStack* callStack;
  const Config* cfg;
  Frame* frame;
  uint8_t* coverage_bitmap;

  Value* locals;
  int locals_count;
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
  // ALWAYS initialize count
  array->array.elements_count = 0;
  int* len = &array->array.elements_count;

  // Initial allocation
  int capacity = 10;
  array->array.elements = malloc(sizeof(Value) * capacity);
  if (!array->array.elements) return 1;

  array->type = type;

  // Skip the type part (I:, C:, etc.)
  char* values = strtok(token, ":_;[]");

  // --- INT ARRAY ---
  if (type->kind == TK_ARRAY && type->array.element_type->kind == TK_INT) {

    while ((values = strtok(NULL, ";[]_")) != NULL) {

      if (*values == '\0') continue;   // skip empty tokens

      if (*len >= capacity) {
        capacity *= 2;
        Value* newbuf = realloc(array->array.elements,
                                sizeof(Value) * capacity);
        if (!newbuf) return 1;
        array->array.elements = newbuf;
      }

      Value value = { .type = TYPE_INT, .data.int_value = strtol(values, NULL, 10) };
      array->array.elements[*len] = value;

      (*len)++;
    }

    // Safe shrink: never realloc(ptr, 0)
    if (*len > 0) {
      Value* shrink = realloc(array->array.elements, sizeof(Value) * (*len));
      if (shrink) array->array.elements = shrink;
    } else {
      free(array->array.elements);
      array->array.elements = NULL;
    }

    return 0;
  }

  // --- CHAR ARRAY ---
  if (type->kind == TK_ARRAY && type->array.element_type->kind == TK_CHAR) {

    while ((values = strtok(NULL, ";[]_'\"")) != NULL) {

      if (*values == '\0') continue;   // skip empty tokens

      if (*len >= capacity) {
        capacity *= 2;
        Value* newbuf = realloc(array->array.elements,
                                sizeof(Value) * capacity);
        if (!newbuf) return 1;
        array->array.elements = newbuf;
      }

      Value value = { .type = TYPE_CHAR, .data.char_value = values[0] };
      array->array.elements[*len] = value;

      (*len)++;
    }

    // Safe shrink
    if (*len > 0) {
      Value* shrink = realloc(array->array.elements, sizeof(Value) * (*len));
      if (shrink) array->array.elements = shrink;
    } else {
      free(array->array.elements);
      array->array.elements = NULL;
    }

    return 0;
  }

  LOG_ERROR("Dont know how to handle this array type interpreter.c: %s", token);
  return 1;
}
static int parse_next_parameter(Type* type, char* token, Value* value)

{
    if (!type || !token || !value) {
        return 1;
    }

    if (type_is_array(type)) {
        ObjectValue* array = malloc(sizeof(ObjectValue));
        array->array.elements_count = 0;
        if (parse_array(type, token, array)) {
            LOG_ERROR("While handling parse array");
            return 10;
        }

        value->type = TYPE_REFERENCE;

        if (heap_insert(array, &value->data.ref_value)) {
            LOG_ERROR("While inserting array into heap");
            return 11;
        }

        return 0;
    }

    if (type == TYPE_INT) {
        value->type = TYPE_INT;
        value->data.int_value = (int)strtol(token, NULL, 10);
        return 0;
    }

    if (type == TYPE_BOOLEAN) {
        value->type = TYPE_BOOLEAN;

        if (strcmp(token, "true") == 0) {
            value->data.bool_value = true;
        } else if (strcmp(token, "false") == 0) {
            value->data.bool_value = false;
        } else {
            LOG_ERROR("Invalid boolean literal: %s", token);
            return 4;
        }

        return 0;
    }

    LOG_ERROR("Unhandled type kind: %d", type->kind);
    return 3;
}

Value* build_locals_from_str(const Method* m, char* parameters, int* locals_count)
{
  *locals_count = 0;

  if (!parameters) {
    LOG_ERROR("build_locals_from_str received NULL parameters");
    return NULL;
  }

  Vector* arg_types = method_get_arguments_as_types(m);
  if (!arg_types) {
    LOG_ERROR("method_get_arguments_as_types returned NULL");
    return NULL;
  }

  const int args_expected = vector_length(arg_types);

  int capacity = args_expected > 0 ? args_expected : 1;
  Value* locals = malloc(capacity * sizeof(Value));
  if (!locals) {
    vector_delete(arg_types);
    return NULL;
  }

  parameter_fix(parameters);

  char* state = NULL;
  char* token = strtok_r(parameters, "(), ", &state);

  while (token && *locals_count < args_expected) {

    Type* type = *(Type**) vector_get(arg_types, *locals_count);
    if (!type) {
      LOG_ERROR("arg_types[%d] is NULL!", *locals_count);
      free(locals);
      vector_delete(arg_types);
      return NULL;
    }

    if (token[0] == '\0') {
      token = strtok_r(NULL, "(), ", &state);
      continue;
    }

    Value v;
    if (parse_next_parameter(type, token, &v)) {
      LOG_ERROR("Failed to parse parameter '%s'", token);
      free(locals);
      vector_delete(arg_types);
      return NULL;
    }

    locals[*locals_count] = v;
    (*locals_count)++;

    token = strtok_r(NULL, "(), ", &state);
  }

  if (*locals_count == 0) {
    free(locals);
    locals = NULL;
  } else {
    Value* shrunk = realloc(locals, *locals_count * sizeof(Value));
    if (shrunk) locals = shrunk;
  }

  vector_delete(arg_types);
  return locals;
}

Value* build_locals_fast(const Method* m,
                         const uint8_t* data,
                         size_t len,
                         int* locals_count)
{
    *locals_count = 0;

    Vector* types = method_get_arguments_as_types(m);
    int argc = vector_length(types);

    if (argc == 0) {
        vector_delete(types);
        return NULL;
    }

    Value* locals = malloc(sizeof(Value) * argc);
    if (!locals) {
        vector_delete(types);
        return NULL;
    }

    size_t pos = 0;

    for (int i = 0; i < argc; i++) {
        Type* t = *(Type**)vector_get(types, i);

        if (pos >= len) break;

        Value v;

        switch (t->kind) {
        case TK_INT: {
            int8_t raw = (int8_t)data[pos++];
            v.type = TYPE_INT;
            v.data.int_value = raw;
            break;
        }

        case TK_BOOLEAN: {
            uint8_t raw = data[pos++];
            v.type = TYPE_BOOLEAN;
            v.data.bool_value = raw & 1;
            break;
        }

        case TK_CHAR: {
            char raw = (char)data[pos++];
            v.type = TYPE_CHAR;
            v.data.char_value = raw;
            break;
        }

        case TK_ARRAY: {
            if (pos >= len) goto fail;
            uint8_t array_len = data[pos++];

            ObjectValue* arr = malloc(sizeof(ObjectValue));
            arr->type = t;
            arr->array.elements_count = array_len;
            arr->array.elements = malloc(sizeof(Value) * array_len);

            Type* inner = t->array.element_type;

            for (int j = 0; j < array_len; j++) {
                if (pos >= len) array_len = j;

                switch (inner->kind) {
                case TK_INT:
                    arr->array.elements[j].type = TYPE_INT;
                    arr->array.elements[j].data.int_value = (int8_t)data[pos++];
                    break;

                case TK_BOOLEAN:
                    arr->array.elements[j].type = TYPE_BOOLEAN;
                    arr->array.elements[j].data.bool_value = data[pos++] & 1;
                    break;

                case TK_CHAR:
                    arr->array.elements[j].type = TYPE_CHAR;
                    arr->array.elements[j].data.char_value = (char)data[pos++];
                    break;

                default:
                    goto fail;
                }
            }

            v.type = TYPE_REFERENCE;
            if (heap_insert(arr, &v.data.ref_value))
                goto fail;

            break;
        }

        default:
            goto fail;
        }

        locals[*locals_count] = v;
        (*locals_count)++;
    }

    vector_delete(types);
    return locals;

fail:
    free(locals);
    vector_delete(types);
    return NULL;
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
    frame->instruction_table = get_instruction_table(m, cfg);
    if (!frame->instruction_table) {
        LOG_ERROR("Failed to build instruction table for method: %s", method_get_id(m));
        free(frame->stack);
        free(frame);
        return NULL;
    }

    return frame;
}

static StepResult handle_load(VMContext* vm_context, Instruction* instruction)
{
    LoadOP* load = &instruction->data.load;
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

static StepResult handle_push(VMContext* vm_context, Instruction* instruction)
{
    PushOP* push = &instruction->data.push;
    if (!push) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    Value value = push->value;
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_binary(VMContext* vm_context, Instruction* instruction)
{
    BinaryOP* binary = &instruction->data.binary;
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

static StepResult handle_get(VMContext* vm_context, Instruction* instruction)
{
    GetOP* get = &instruction->data.get;
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

static StepResult handle_return(VMContext* vm_context, Instruction* instruction)
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

static StepResult handle_ifz(VMContext* vm_context, Instruction* instruction)
{
    IfOP* ift = &instruction->data.ift;
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

static StepResult handle_ift(VMContext* vm_context, Instruction* instruction)
{
    IfOP* ift = &instruction->data.ift;
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

static StepResult handle_store(VMContext* vm_context, Instruction* instruction)
{
    StoreOP* store = &instruction->data.store;
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

static StepResult handle_goto(VMContext* vm_context, Instruction* instruction)
{
    GotoOP* go2 = &instruction->data.go2;
    if (!go2) {
        return SR_NULL_INSTRUCTION;
    }

    Frame* frame = vm_context->frame;
    frame->pc = go2->target;
    return SR_OK;
}

static StepResult handle_dup(VMContext* vm_context, Instruction* instruction)
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

static StepResult handle_invoke(VMContext* vm_context, Instruction* instruction)
{
    StepResult result = SR_OK;

    InvokeOP* invoke = &instruction->data.invoke;
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
static StepResult handle_array_length(VMContext* vm_context, Instruction* instruction)
{
    ArrayLengthOP* array_length = &instruction->data.array_length;
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

static StepResult handle_new_array(VMContext* vm_context, Instruction* instruction)
{
    NewArrayOP* new_array = &instruction->data.new_array;
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

static StepResult handle_array_store(VMContext* vm_context, Instruction* instruction)
{
    ArrayStoreOP* array_store = &instruction->data.array_store;
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

static StepResult handle_array_load(VMContext* vm_context, Instruction* instruction)
{
    ArrayLoadOP* array_load = &instruction->data.array_load;
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

static StepResult handle_incr(VMContext* vm_context, Instruction* instruction)
{
    IncrOP* incr = &instruction->data.incr;
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

static StepResult handle_skip(VMContext* vm_context, Instruction* instruction)
{
    Frame* frame = vm_context->frame;
    frame->pc++;

    return SR_OK;
}

static StepResult handle_throw(VMContext* vm_context, Instruction* instruction)
{
    return SR_ASSERTION_ERR;
}

static Instruction* get_instruction(VMContext* vm_context)
{
    Frame* frame = vm_context->frame;
    InstructionTable* instruction_table = frame->instruction_table;
    if (!instruction_table) {
        return NULL;
    }
    if (frame->pc < 0 || frame->pc >= instruction_table->count) {
        return NULL;
    }
    return instruction_table->instructions[frame->pc];
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
    Frame* frame = vm_context->frame;

    if (vm_context->coverage_bitmap) {
        coverage_mark_thread(vm_context->coverage_bitmap, (uint32_t)frame->pc);
    }

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

    Instruction* instruction = get_instruction(vm_context);
    if (!instruction) {
        return SR_NULL_INSTRUCTION;
    }
    // LOG_DEBUG("Interpreting: %s", opcode_print(instruction->opcode));

    Opcode opcode = instruction->opcode;
    if (opcode < 0 || opcode >= OP_COUNT) {
        return SR_UNKNOWN_OPCODE;
    }

    return opcode_table[instruction->opcode](vm_context, instruction);
}




VMContext* interpreter_setup(const Method* m, const Options* opts, const Config* cfg, uint8_t* thread_bitmap)
{
    if (!m || !opts || !cfg) {
        return NULL;
    }

    const char* raw_params = opts->parameters ? opts->parameters : "";
    char* parameters = strdup(raw_params);
    LOG_DEBUG("BUILDING FRAME...");
    LOG_DEBUG("PARAMETERS: %s", parameters);

    int locals_count = 0;
    Value* locals = build_locals_from_str(m, parameters, &locals_count);
    free(parameters);

    if (!locals) {
      printf("FATAL: locals_str is NULL\n");
      exit(1);
    }

    Frame* frame = build_frame(m, cfg, locals, locals_count);
    LOG_DEBUG("LOCALS COUNT: %d", locals_count);

    if (!frame) {
        return NULL;
    }

    CallStack* call_stack = calloc(1, sizeof(CallStack));
    call_stack->top = NULL;
    call_stack->count = 0;
    call_stack_push(call_stack, frame);

    VMContext* vm_context = malloc(sizeof(VMContext));
    vm_context->call_stack = call_stack;
    vm_context->frame = frame;
    vm_context->cfg = cfg;

    vm_context->coverage_bitmap = thread_bitmap;



    return vm_context;
}

void VMContext_set_locals(VMContext* vm, Value* new_locals, int count)
{
    Frame* f = vm->frame;

    free(f->locals);
    f->locals = new_locals;
    f->locals_count = count;
}

static Frame* build_persistent_frame(const Method* m,
                                     const Config* cfg,
                                     Value* locals,
                                     int locals_count)
{
    Frame* frame = calloc(1, sizeof(Frame));
    if (!frame) return NULL;

    frame->pc = 0;
    frame->locals = locals;
    frame->locals_count = locals_count;

    frame->stack = stack_new();   // persistent, reused, cleared manually
    if (!frame->stack) {
        free(frame);
        return NULL;
    }

    frame->instruction_table = get_instruction_table(m, cfg);
    if (!frame->instruction_table) {
        stack_delete(frame->stack);
        free(frame);
        return NULL;
    }

    return frame;
}

void VMContext_reset(VMContext* vm)
{
    if (!vm || !vm->frame || !vm->call_stack) return;

    Frame* f = vm->frame;
    f->pc = 0;
    stack_clear(f->stack);

    CallStack* cs = vm->call_stack;

    while (cs->top) {
        CallStackNode* n = cs->top;
        cs->top = n->next;
        free(n);
    }
    cs->count = 0;

    call_stack_push(cs, f);
}

static void frame_free(Frame* frame) {
    if (!frame) return;

    free(frame->locals);

    stack_delete(frame->stack);

    free(frame);
}

VMContext* persistent_interpreter_setup(const Method* m,
                                        const Options* opts,
                                        const Config* cfg,
                                        uint8_t* thread_bitmap)
{
    if (!m || !opts || !cfg) return NULL;

    int locals_count = 0;
    Value* locals = NULL;

    Frame* frame = build_persistent_frame(m, cfg, locals, locals_count);
    if (!frame) {
        return NULL;
    }

    CallStack* call_stack = calloc(1, sizeof(CallStack));
    if (!call_stack) {
        frame_free(frame);
        return NULL;
    }

    call_stack_push(call_stack, frame);

    VMContext* vm = calloc(1, sizeof(VMContext));
    if (!vm) {
        while (call_stack->top) {
            CallStackNode* n = call_stack->top;
            call_stack->top = n->next;
            free(n->frame);
            free(n);
        }
        free(call_stack);
        return NULL;
    }

    vm->call_stack = call_stack;
    vm->frame = frame;
    vm->cfg = cfg;
    vm->coverage_bitmap = thread_bitmap;

    return vm;
}

void dump_locals(Value *locals, int locals_count)
{
  printf("[FUZZ] locals_count = %d\n", locals_count);

  for (int i = 0; i < locals_count; i++) {
    Value v = locals[i];

    if (!v.type) {
      printf("  local[%d] = <NULL TYPE>\n", i);
      continue;
    }

    switch (v.type->kind) {
    case TK_INT:
      printf("  local[%d] = INT %d\n", i, v.data.int_value);
      break;

    case TK_BOOLEAN:
      printf("  local[%d] = BOOL %d\n", i, v.data.bool_value);
      break;

    case TK_CHAR:
      printf("  local[%d] = CHAR '%c' (%d)\n", i,
             v.data.char_value, v.data.char_value);
      break;

    case TK_ARRAY:
      // value itself is a raw array, not a reference
      printf("  local[%d] = <RAW ARRAY VALUE>\n", i);
      break;

    case TK_REFERENCE: {
      ObjectValue *obj = heap_get(v.data.ref_value);
      if (!obj) {
        printf("  local[%d] = <NULL REF>\n", i);
        break;
      }

      if (obj->type && obj->type->kind == TK_ARRAY) {
        printf("  local[%d] = ARRAY(len=%d): ",
               i, obj->array.elements_count);

        for (int j = 0; j < obj->array.elements_count; j++) {
          Value e = obj->array.elements[j];

          if (!e.type) {
            printf("<?>");
            continue;
          }

          switch (e.type->kind) {
          case TK_INT:
            printf("%d,", e.data.int_value);
            break;
          case TK_CHAR:
            printf("'%c',", e.data.char_value);
            break;
          case TK_BOOLEAN:
            printf("%d,", e.data.bool_value);
            break;
          default:
            printf("<?>");
            break;
          }
        }
        printf("\n");
      } else {
        printf("  local[%d] = <OBJ REF kind=%d>\n",
               i, obj->type ? obj->type->kind : -1);
      }
      break;
    }

    default:
      printf("  local[%d] = <UNKNOWN TYPE %d>\n", i, v.type->kind);
      break;
    }
  }
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

size_t interpreter_instruction_count(const Method* m, const Config* cfg)
{
  InstructionTable* table = get_instruction_table(m, cfg);
  return table ? table->count : 0;
}

void interpreter_free(VMContext* vm) {
    if (!vm) return;

    CallStack* cs = vm->call_stack;

    if (cs) {
        CallStackNode* node = cs->top;

        while (node) {
            CallStackNode* next = node->next;

            // free the frame object
            frame_free(node->frame);

            // free the stack node
            free(node);

            node = next;
        }

        free(cs);
    }


    free(vm);
}

void instruction_table_map_free() {
  ITItem* it = it_map;
  while (it) {
    ITItem* next = it->next;

    instruction_table_free(it->instruction_table);

    free(it);
    it = next;
  }

  it_map = NULL;
}

void VMContext_set_coverage_bitmap(VMContext* vm, uint8_t* bitmap)
{
    if (!vm) return;
    vm->coverage_bitmap = bitmap;
}