#include "interpreter_fuzz.h"
#include "coverage.h"
#include "stack.h"
#include "ir_function.h"
#include "heap.h"
#include "log.h"
#include "ir_instruction.h"
#include "ir_program.h"
#include "utils.h"
#include "type.h"
#include "opcode.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define ITERATION 100000

typedef struct {
    Value*      locals;
    Stack*      stack;
    IrFunction* ir_function;
    int         locals_count;
    int         pc;
} Frame;

typedef struct CallStackNode {
    Frame*                  frame;
    struct CallStackNode*   next;
} CallStackNode;

typedef struct {
    CallStackNode*  top;
    int             count;
} CallStack;

struct VMContext {
    CallStack*     call_stack;
    const Config*  cfg;
    Frame*         frame;
    uint8_t*       coverage_bitmap;
    Heap*          heap;
};

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
        [SR_OK]               = "SR_OK",
        [SR_OUT_OF_BOUNDS]    = "SR_OUT_OF_BOUNDS",
        [SR_NULL_INSTRUCTION] = "SR_NULL_INSTRUCTION",
        [SR_NULL_POINTER]     = "SR_NULL_POINTER",
        [SR_EMPTY_STACK]      = "SR_EMPTY_STACK",
        [SR_DIVIDE_BY_ZERO]   = "SR_DIVIDE_BY_ZERO",
        [SR_BO_UNKNOWN]       = "SR_BO_UNKNOWN",
        [SR_BO_MUL_GEN_ERR]   = "SR_BO_MUL_GEN_ERR",
        [SR_BO_DIV_DBZ]       = "SR_BO_DIV_DBZ",
        [SR_BO_DIV_GEN_ERR]   = "SR_BO_DIV_GEN_ERR",
        [SR_BO_ADD_GEN_ERR]   = "SR_BO_ADD_GEN_ERR",
        [SR_BO_SUB_GEN_ERR]   = "SR_BO_SUB_GEN_ERR",
        [SR_BO_REM_GEN_ERR]   = "SR_BO_REM_GEN_ERR",
        [SR_UNKNOWN_OPCODE]   = "SR_UNKNOWN_OPCODE",
        [SR_ASSERTION_ERR]    = "SR_ASSERTION_ERR",
        [SR_INVALID_TYPE]     = "SR_INVALID_TYPE",
        [SR_INTERNAL_NULL_ERR]= "SR_INTERNAL_NULL_ERR",
};

typedef StepResult (*OpHandler)(VMContext*, IrInstruction*);

/* --------------------------------------------------------------------------
 * Frame / call stack helpers
 * -------------------------------------------------------------------------- */

static Frame* build_persistent_frame(const Method* m,
                                     const Config* cfg,
                                     Value* locals,
                                     int locals_count)
{
    Frame* frame = calloc(1, sizeof(Frame));
    if (!frame)
        return NULL;

    frame->pc           = 0;
    frame->locals       = locals;
    frame->locals_count = locals_count;

    frame->stack = stack_new(); // persistent, reused, cleared manually
    if (!frame->stack) {
        free(frame);
        return NULL;
    }

    frame->ir_function = ir_program_get_function_ir(m, cfg);
    if (!frame->ir_function) {
        LOG_ERROR("Persistent table lookup failed for method: %s",
                  method_get_id(m));
        printf("[LOOKUP] searching for %s\n", method_get_id(m));
        stack_delete(frame->stack);
        free(frame);
        return NULL;
    }

    return frame;
}

static void frame_free(Frame* frame)
{
    if (!frame)
        return;

    free(frame->locals);
    stack_delete(frame->stack);
    free(frame);
}

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
    new_node->next  = call_stack->top;

    call_stack->top = new_node;
    call_stack->count++;
}

static Frame* call_stack_pop(CallStack* call_stack)
{
    if (!call_stack || !call_stack->top)
        return NULL;

    CallStackNode* to_pop = call_stack->top;
    Frame*         popped = to_pop->frame;
    call_stack->top       = to_pop->next;
    call_stack->count--;

    free(to_pop);
    return popped;
}

static Frame* call_stack_peek(CallStack* call_stack)
{
    if (!call_stack || !call_stack->top)
        return NULL;
    return call_stack->top->frame;
}

/* --------------------------------------------------------------------------
 * Helpers used by some op handlers
 * -------------------------------------------------------------------------- */

static Value* build_locals_from_frame(const Method* m,
                                      const Frame* frame,
                                      int* out_count)
{
    (void)m; // currently unused, kept for symmetry / future use

    if (!frame || !out_count) {
        LOG_ERROR("build_locals_from_frame: null arguments");
        return NULL;
    }

    int count = frame->locals_count;
    *out_count = 0;

    if (count <= 0)
        return NULL;

    if (!frame->locals) {
        LOG_ERROR("build_locals_from_frame: frame has locals_count > 0 but locals == NULL");
        return NULL;
    }

    Value* out = malloc(sizeof(Value) * (size_t)count);
    if (!out) {
        LOG_ERROR("build_locals_from_frame: malloc failed");
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        out[i] = frame->locals[i];
    }

    *out_count = count;
    return out;
}

/* --------------------------------------------------------------------------
 * Opcode handlers
 * -------------------------------------------------------------------------- */

static StepResult handle_load(VMContext* vm_context, IrInstruction* ir_instruction)
{
    LoadOP* load = &ir_instruction->data.load;
    if (!load)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    int index = load->index;
    if (index < 0 || index >= frame->locals_count)
        return SR_OUT_OF_BOUNDS;

    Value value = frame->locals[index];
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_push(VMContext* vm_context, IrInstruction* ir_instruction)
{
    PushOP* push = &ir_instruction->data.push;
    if (!push)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value value  = push->value;
    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_binary(VMContext* vm_context, IrInstruction* instruction)
{
    BinaryOP* binary = &instruction->data.binary;
    if (!binary)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value lhs, rhs, result;

    if (stack_pop(frame->stack, &rhs))
        return SR_EMPTY_STACK;
    if (stack_pop(frame->stack, &lhs))
        return SR_EMPTY_STACK;

    switch (binary->op) {
        case BO_MUL:
            if (value_mul(&lhs, &rhs, &result))
                return SR_BO_MUL_GEN_ERR;
            break;
        case BO_ADD:
            if (value_add(&lhs, &rhs, &result))
                return SR_BO_ADD_GEN_ERR;
            break;
        case BO_DIV:
            switch (value_div(&lhs, &rhs, &result)) {
                case BO_OK:
                    break;
                case BO_DIVIDE_BY_ZERO:
                    return SR_BO_DIV_DBZ;
                default:
                    return SR_BO_DIV_GEN_ERR;
            }
            break;
        case BO_SUB:
            if (value_sub(&lhs, &rhs, &result))
                return SR_BO_SUB_GEN_ERR;
            break;
        case BO_REM:
            if (value_rem(&lhs, &rhs, &result))
                return SR_BO_REM_GEN_ERR;
            break;
        default:
            return SR_BO_UNKNOWN;
    }

    stack_push(frame->stack, result);
    frame->pc++;
    return SR_OK;
}

static StepResult handle_get(VMContext* vm_context, IrInstruction* instruction)
{
    (void)instruction;

    Frame* frame = vm_context->frame;
    Value value;
    value.type = TYPE_BOOLEAN;
    value.data.bool_value = false;

    stack_push(frame->stack, value);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_return(VMContext* vm_context, IrInstruction* instruction)
{
    (void)instruction;

    Frame*     frame      = vm_context->frame;
    CallStack* call_stack = vm_context->call_stack;

    Value value;
    if (stack_pop(frame->stack, &value))
        value.type = NULL; // no value, but we still unwind

    call_stack_pop(call_stack);

    if (call_stack->count > 0) {
        Frame* caller = call_stack_peek(call_stack);
        if (caller && value.type)
            stack_push(caller->stack, value);
    }

    return SR_OK;
}

static bool handle_ift_aux(IfCondition condition, int value1, int value2)
{
    switch (condition) {
        case IF_EQ: return value1 == value2;
        case IF_NE: return value1 != value2;
        case IF_GT: return value1 > value2;
        case IF_LT: return value1 < value2;
        case IF_GE: return value1 >= value2;
        case IF_LE: return value1 <= value2;
        default:    return false;
    }
}

static StepResult handle_ifz(VMContext* vm_context, IrInstruction* instruction)
{
    IfOP* ift = &instruction->data.ift;
    if (!ift)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value  value;

    if (stack_pop(frame->stack, &value))
        return SR_EMPTY_STACK;

    bool result;

    if (value.type == TYPE_INT)
        result = handle_ift_aux(ift->condition, value.data.int_value, 0);
    else if (value.type == TYPE_CHAR)
        result = handle_ift_aux(ift->condition, value.data.char_value, 0);
    else if (value.type == TYPE_BOOLEAN)
        result = handle_ift_aux(ift->condition, value.data.bool_value, 0);
    else
        return SR_INVALID_TYPE;

    frame->pc = result ? ift->target : frame->pc + 1;
    return SR_OK;
}

static StepResult handle_ift(VMContext* vm_context, IrInstruction* instruction)
{
    IfOP* ift = &instruction->data.ift;
    if (!ift)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value  v1, v2;

    if (stack_pop(frame->stack, &v2))
        return SR_EMPTY_STACK;
    if (stack_pop(frame->stack, &v1))
        return SR_EMPTY_STACK;

    int a, b;

    if (v1.type == TYPE_INT)
        a = v1.data.int_value;
    else if (v1.type == TYPE_BOOLEAN)
        a = (int)v1.data.bool_value;
    else if (v1.type == TYPE_CHAR)
        a = (int)v1.data.char_value;
    else
        return SR_INVALID_TYPE;

    if (v2.type == TYPE_INT)
        b = v2.data.int_value;
    else if (v2.type == TYPE_BOOLEAN)
        b = (int)v2.data.bool_value;
    else if (v2.type == TYPE_CHAR)
        b = (int)v2.data.char_value;
    else
        return SR_INVALID_TYPE;

    bool result = handle_ift_aux(ift->condition, a, b);
    frame->pc = result ? ift->target : frame->pc + 1;
    return SR_OK;
}

static StepResult handle_store(VMContext* vm_context, IrInstruction* instruction)
{
    StoreOP* store = &instruction->data.store;
    if (!store)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value  v;

    if (stack_pop(frame->stack, &v))
        return SR_EMPTY_STACK;

    int index = store->index;
    if (index < 0)
        return SR_OUT_OF_BOUNDS;

    if (index >= frame->locals_count) {
        int new_count = index + 1;
        Value* new_locals = realloc(frame->locals, new_count * sizeof(Value));
        if (!new_locals)
            return SR_INTERNAL_NULL_ERR;
        // zero-initialize newly exposed slots
        for (int i = frame->locals_count; i < new_count; i++) {
            new_locals[i].type = NULL;
        }
        frame->locals       = new_locals;
        frame->locals_count = new_count;
    }

    frame->locals[index] = v;
    frame->pc++;
    return SR_OK;
}

static StepResult handle_goto(VMContext* vm_context, IrInstruction* instruction)
{
    GotoOP* go2 = &instruction->data.go2;
    if (!go2)
        return SR_NULL_INSTRUCTION;

    vm_context->frame->pc = go2->target;
    return SR_OK;
}

static StepResult handle_dup(VMContext* vm_context, IrInstruction* instruction)
{
    (void)instruction;

    Frame* frame = vm_context->frame;
    Value* top   = stack_peek(frame->stack);

    if (top)
        stack_push(frame->stack, *top);

    frame->pc++;
    return SR_OK;
}

static StepResult handle_invoke(VMContext* vm_context, IrInstruction* instruction)
{
    StepResult result    = SR_OK;
    Method*    m         = NULL;
    char*      method_id = NULL;

    InvokeOP* invoke = &instruction->data.invoke;
    if (!invoke)
        return SR_NULL_INSTRUCTION;

    Frame*        frame      = vm_context->frame;
    CallStack*    call_stack = vm_context->call_stack;
    const Config* cfg        = vm_context->cfg;

    method_id = get_method_signature(invoke);
    if (!method_id) {
        // Could not build a signature -> external call, just skip.
        frame->pc++;
        return SR_OK;
    }

    m = method_create(method_id);
    if (!m) {
        result = SR_INTERNAL_NULL_ERR;
        goto cleanup;
    }

    char* dot = strchr(method_id, '.');
    if (dot) {
        *dot = '\0';
    }
    char* root = method_id;
    if (!root || !*root) {
        result = SR_INTERNAL_NULL_ERR;
        goto cleanup;
    }

    if (strcmp(root, "jpamb") == 0) {
        int    locals_count = 0;
        Value* locals       = build_locals_from_frame(m, frame, &locals_count);
        if (!locals && locals_count > 0) {
            result = SR_INTERNAL_NULL_ERR;
            goto cleanup;
        }

        Frame* new_frame = calloc(1, sizeof(Frame));
        if (!new_frame) {
            free(locals);
            result = SR_INTERNAL_NULL_ERR;
            goto cleanup;
        }

        new_frame->pc           = 0;
        new_frame->locals       = locals;
        new_frame->locals_count = locals_count;
        new_frame->stack        = stack_new();
        if (!new_frame->stack) {
            free(new_frame->locals);
            free(new_frame);
            result = SR_INTERNAL_NULL_ERR;
            goto cleanup;
        }

        new_frame->ir_function = ir_program_get_function_ir(m, cfg);
        if (!new_frame->ir_function) {
            LOG_ERROR("Failed to build IR function for invoked method");
            stack_delete(new_frame->stack);
            free(new_frame->locals);
            free(new_frame);
            result = SR_INTERNAL_NULL_ERR;
            goto cleanup;
        }

        call_stack_push(call_stack, new_frame);
    }

    cleanup:
    if (m)
        method_delete(m);
    free(method_id);

    frame->pc++;
    return result;
}

static StepResult handle_array_length(VMContext* vm_context, IrInstruction* instruction)
{
    ArrayLengthOP* array_length = &instruction->data.array_length;
    (void)array_length;

    Frame* frame = vm_context->frame;
    Value  value;

    if (stack_pop(frame->stack, &value))
        return SR_EMPTY_STACK;

    if (value.type != TYPE_REFERENCE)
        return SR_INVALID_TYPE;

    ObjectValue* array = heap_get(vm_context->heap, value.data.ref_value);
    if (!array)
        return SR_NULL_POINTER;

    if (array->type->kind != TK_ARRAY)
        return SR_INVALID_TYPE;

    Value out = { .type = TYPE_INT };
    out.data.int_value = array->array.elements_count;

    stack_push(frame->stack, out);
    frame->pc++;
    return SR_OK;
}

static StepResult handle_new_array(VMContext* vm_context, IrInstruction* instruction)
{
    NewArrayOP* new_array = &instruction->data.new_array;
    if (!new_array)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value  size;

    if (stack_pop(frame->stack, &size))
        return SR_EMPTY_STACK;

    if (size.type != TYPE_INT)
        return SR_INVALID_TYPE;
    if (size.data.int_value < 0)
        return SR_OUT_OF_BOUNDS;

    int len = size.data.int_value;

    ObjectValue* array = malloc(sizeof(ObjectValue));
    if (!array)
        return SR_INTERNAL_NULL_ERR;

    array->type                 = make_array_type(new_array->type);
    array->array.elements_count = len;
    array->array.elements       = NULL;

    if (len > 0) {
        array->array.elements = calloc((size_t)len, sizeof(Value));
        if (!array->array.elements) {
            free(array);
            return SR_INTERNAL_NULL_ERR;
        }
    }

    Value ref = { .type = TYPE_REFERENCE };
    if (heap_insert(vm_context->heap, array, &ref.data.ref_value)) {
        if (array->array.elements)
            free(array->array.elements);
        free(array);
        return SR_OUT_OF_BOUNDS;
    }

    stack_push(frame->stack, ref);
    frame->pc++;
    return SR_OK;
}

static StepResult handle_array_store(VMContext* vm_context, IrInstruction* instruction)
{
    ArrayStoreOP* array_store = &instruction->data.array_store;
    if (!array_store)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;

    Value value, index, ref;

    if (stack_pop(frame->stack, &value))
        return SR_EMPTY_STACK;
    if (stack_pop(frame->stack, &index))
        return SR_EMPTY_STACK;
    if (stack_pop(frame->stack, &ref))
        return SR_EMPTY_STACK;

    if (index.type != TYPE_INT)
        return SR_INVALID_TYPE;

    ObjectValue* array = heap_get(vm_context->heap, ref.data.ref_value);
    if (!array)
        return SR_NULL_POINTER;

    if (array->type->kind != TK_ARRAY)
        return SR_INVALID_TYPE;
    if (array->type->array.element_type != array_store->type ||
        value.type != array_store->type)
        return SR_INVALID_TYPE;

    if (index.data.int_value < 0 ||
        index.data.int_value >= array->array.elements_count)
        return SR_OUT_OF_BOUNDS;

    array->array.elements[index.data.int_value] = value;
    frame->pc++;

    return SR_OK;
}

static StepResult handle_array_load(VMContext* vm_context, IrInstruction* instruction)
{
    ArrayLoadOP* array_load = &instruction->data.array_load;
    if (!array_load)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value  index, ref;

    if (stack_pop(frame->stack, &index))
        return SR_EMPTY_STACK;
    if (stack_pop(frame->stack, &ref))
        return SR_EMPTY_STACK;

    if (index.type != TYPE_INT)
        return SR_INVALID_TYPE;

    ObjectValue* array = heap_get(vm_context->heap, ref.data.ref_value);
    if (!array)
        return SR_NULL_POINTER;

    if (array->type->kind != TK_ARRAY ||
        array->type->array.element_type != array_load->type)
        return SR_INVALID_TYPE;

    if (index.data.int_value < 0 ||
        index.data.int_value >= array->array.elements_count)
        return SR_OUT_OF_BOUNDS;

    Value v = array->array.elements[index.data.int_value];
    stack_push(frame->stack, v);
    frame->pc++;

    return SR_OK;
}

static StepResult handle_incr(VMContext* vm_context, IrInstruction* instruction)
{
    IncrOP* incr = &instruction->data.incr;
    if (!incr)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;

    if (incr->index < 0 || incr->index >= frame->locals_count)
        return SR_OUT_OF_BOUNDS;

    Value* v = &frame->locals[incr->index];

    if (v->type != TYPE_INT)
        return SR_INVALID_TYPE;

    v->data.int_value += incr->amount;
    frame->pc++;
    return SR_OK;
}

static StepResult handle_skip(VMContext* vm_context, IrInstruction* instruction)
{
    (void)instruction;

    vm_context->frame->pc++;
    return SR_OK;
}

static StepResult handle_throw(VMContext* vm_context, IrInstruction* instruction)
{
    (void)instruction;

    vm_context->frame->pc++;
    return SR_ASSERTION_ERR;
}

static StepResult handle_negate(VMContext* vm_context, IrInstruction* ir_instruction)
{
    NegateOP* negate = &ir_instruction->data.negate;
    if (!negate)
        return SR_NULL_INSTRUCTION;

    Frame* frame = vm_context->frame;
    Value  v;

    if (stack_pop(frame->stack, &v))
        return SR_EMPTY_STACK;

    if (v.type != negate->type)
        return SR_INVALID_TYPE;

    switch (v.type->kind) {
        case TK_INT:
            v.data.int_value = -v.data.int_value;
            break;
        case TK_BOOLEAN:
            v.data.bool_value = !v.data.bool_value;
            break;
        case TK_CHAR:
            v.data.char_value = (char)(- (int)v.data.char_value);
            break;
        default:
            return SR_INVALID_TYPE;
    }

    stack_push(frame->stack, v);
    frame->pc++;
    return SR_OK;
}

static OpHandler opcode_table[OP_COUNT] = {
        [OP_LOAD]        = handle_load,
        [OP_PUSH]        = handle_push,
        [OP_BINARY]      = handle_binary,
        [OP_GET]         = handle_get,
        [OP_RETURN]      = handle_return,
        [OP_IF_ZERO]     = handle_ifz,
        [OP_IF]          = handle_ift,
        [OP_NEW]         = handle_skip,
        [OP_DUP]         = handle_dup,
        [OP_INVOKE]      = handle_invoke,
        [OP_THROW]       = handle_throw,
        [OP_STORE]       = handle_store,
        [OP_GOTO]        = handle_goto,
        [OP_CAST]        = handle_skip,
        [OP_NEW_ARRAY]   = handle_new_array,
        [OP_ARRAY_LOAD]  = handle_array_load,
        [OP_ARRAY_STORE] = handle_array_store,
        [OP_ARRAY_LENGTH]= handle_array_length,
        [OP_INCR]        = handle_incr,
        [OP_NEGATE]      = handle_negate,
};

static IrInstruction* get_instruction(VMContext* vm_context)
{
    Frame*      frame = vm_context->frame;
    IrFunction* fn    = frame->ir_function;

    if (!fn)
        return NULL;

    if (frame->pc < 0 || frame->pc >= (int)vector_length(fn->ir_instructions))
        return NULL;

    return *(IrInstruction**)vector_get(fn->ir_instructions, (size_t)frame->pc);
}

/* --------------------------------------------------------------------------
 * VMContext setup / run / teardown
 * -------------------------------------------------------------------------- */

VMContext* fuzz_interpreter_setup(const Method* m,
                                  const Options* opts,
                                  const Config* cfg,
                                  uint8_t* thread_bitmap)
{
    (void)opts;

    if (!m || !cfg)
        return NULL;

    int    locals_count = 0;
    Value* locals       = NULL;

    Frame* frame = build_persistent_frame(m, cfg, locals, locals_count);
    if (!frame)
        return NULL;

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
            call_stack->top  = n->next;
            frame_free(n->frame);
            free(n);
        }
        free(call_stack);
        return NULL;
    }

    vm->heap = heap_create();
    if (!vm->heap) {
        while (call_stack->top) {
            CallStackNode* n = call_stack->top;
            call_stack->top  = n->next;
            frame_free(n->frame);
            free(n);
        }
        free(call_stack);
        free(vm);
        return NULL;
    }

    vm->call_stack      = call_stack;
    vm->frame           = frame;
    vm->cfg             = cfg;
    vm->coverage_bitmap = thread_bitmap;

    return vm;
}

void fuzz_VMContext_set_locals(VMContext* vm, Value* new_locals, int count)
{
    if (!vm || !vm->frame)
        return;

    Frame* f = vm->frame;

    free(f->locals);
    f->locals       = new_locals;
    f->locals_count = count;
}

void fuzz_VMContext_reset(VMContext* vm)
{
    if (!vm || !vm->frame || !vm->call_stack)
        return;

    Frame* root = vm->frame;

    root->pc = 0;
    stack_clear(root->stack);

    CallStack*     cs   = vm->call_stack;
    CallStackNode* node = cs->top;

    while (node) {
        CallStackNode* next = node->next;

        if (node->frame && node->frame != root)
            frame_free(node->frame);

        free(node);
        node = next;
    }

    cs->top   = NULL;
    cs->count = 0;

    call_stack_push(cs, root);
}

static StepResult step(VMContext* vm_context)
{
    vm_context->frame = call_stack_peek(vm_context->call_stack);
    Frame* frame      = vm_context->frame;

    if (!frame)
        return SR_EMPTY_STACK;

    if (!vm_context->cfg)
        return SR_INTERNAL_NULL_ERR;

    if (!vm_context->call_stack)
        return SR_EMPTY_STACK;

    IrInstruction* instruction = get_instruction(vm_context);
    if (!instruction)
        return SR_NULL_INSTRUCTION;

    if (vm_context->coverage_bitmap) {
        uint32_t pc  = (uint32_t)frame->pc;
        uint32_t cap = (uint32_t)vector_length(frame->ir_function->ir_instructions);

        if (pc < cap) {
            coverage_mark_thread(vm_context->coverage_bitmap, pc);
        }
    }

    Opcode opcode = instruction->opcode;
    if (opcode < 0 || opcode >= OP_COUNT) {
        LOG_ERROR("Opcode: %d", opcode);
        return SR_UNKNOWN_OPCODE;
    }

    OpHandler handler = opcode_table[opcode];
    if (!handler)
        return SR_UNKNOWN_OPCODE;

    return handler(vm_context, instruction);
}

RuntimeResult fuzz_interpreter_run(VMContext* vm_context)
{
    RuntimeResult result = RT_OK;
    CallStack*    cs     = vm_context->call_stack;

    if (!cs) {
        LOG_ERROR("CallStack is null");
        goto cleanup;
    }

    int i = 0;
    for (; i < ITERATION; i++) {
        if (cs->count > 0) {
            StepResult sr = step(vm_context);
            switch (sr) {
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
                    LOG_ERROR("%s", step_result_signature[sr]);
                    break;
            }

            goto cleanup;
        } else {
            result = RT_OK;
            goto cleanup;
        }
    }

    cleanup:
    if (result)
        return result;

    if (i == ITERATION)
        return RT_INFINITE;

    return result;
}

void fuzz_VMContext_set_coverage_bitmap(VMContext* vm, uint8_t* bitmap)
{
    if (!vm)
        return;
    vm->coverage_bitmap = bitmap;
}

Heap* fuzz_VMContext_get_heap(VMContext* vm)
{
    if (!vm)
        return NULL;
    return vm->heap;
}

void fuzz_interpreter_free(VMContext* vm)
{
    if (!vm)
        return;

    if (vm->heap)
        heap_free(vm->heap);

    if (vm->frame)
        frame_free(vm->frame);

    free(vm);
}


Value* fuzz_build_locals_fast(Heap* heap,
                              const Method* m,
                              const uint8_t* data,
                              size_t len,
                              int* locals_count)
{
    *locals_count = 0;

    if (!heap || !m || (!data && len > 0))
        return NULL;

    Vector* types = method_get_arguments_as_types(m);
    if (!types)
        return NULL;

    int argc = (int)vector_length(types);
    if (argc == 0) {
        vector_delete(types);
        return NULL;
    }

    Value* locals = calloc((size_t)argc, sizeof(Value));
    if (!locals) {
        vector_delete(types);
        return NULL;
    }

    size_t pos = 0;

    for (int i = 0; i < argc; i++) {
        Type* t = *(Type**)vector_get(types, i);
        if (!t) goto fail;

        Value v; v.type = NULL;

        switch (t->kind) {

            case TK_INT: {
                if (pos + 1 > len) goto fail;
                int8_t b = (int8_t)data[pos++];
                v.type = TYPE_INT;
                v.data.int_value = (int32_t)b;
                break;
            }

            case TK_BOOLEAN: {
                if (pos + 1 > len) goto fail;
                v.type = TYPE_BOOLEAN;
                v.data.bool_value = data[pos++] & 1;
                break;
            }

            case TK_CHAR: {
                if (pos + 1 > len) goto fail;
                v.type = TYPE_CHAR;
                v.data.char_value = (char)data[pos++];
                break;
            }

            case TK_ARRAY: {
                if (pos + 1 > len) goto fail;
                uint8_t arr_len = data[pos++];

                ObjectValue* arr = malloc(sizeof(ObjectValue));
                if (!arr) goto fail;

                arr->type = t;
                arr->array.elements_count = arr_len;
                arr->array.elements = arr_len > 0 ? malloc(arr_len * sizeof(Value)) : NULL;

                if (arr_len > 0 && !arr->array.elements) {
                    free(arr);
                    goto fail;
                }

                for (int j = 0; j < arr_len; j++) {
                    if (pos + 1 > len) {
                        if (arr->array.elements) free(arr->array.elements);
                        free(arr);
                        goto fail;
                    }
                    int8_t b = (int8_t)data[pos++];

                    Value elem;
                    elem.type = TYPE_INT;
                    elem.data.int_value = (int32_t)b;
                    arr->array.elements[j] = elem;
                }

                v.type = TYPE_REFERENCE;
                if (heap_insert(heap, arr, &v.data.ref_value) != 0) {
                    if (arr->array.elements) free(arr->array.elements);
                    free(arr);
                    goto fail;
                }

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
    vector_delete(types);
    free(locals);
    *locals_count = 0;
    return NULL;
}

