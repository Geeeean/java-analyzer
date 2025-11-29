#include "ir_function.h"

#include "cJSON/cJSON.h"
#include "log.h"
#include "method.h"

static IrFunction*
parse_bytecode(cJSON* method)
{
    IrFunction* ir_function = calloc(1, sizeof(*ir_function));
    if (!ir_function) {
        goto cleanup;
    }

    ir_function->capacity = IR_FUNCTION_SIZE;
    ir_function->count    = 0;

    cJSON* code = cJSON_GetObjectItem(method, "code");
    if (!code) {
        goto cleanup;
    }

    cJSON* bytecode = cJSON_GetObjectItem(code, "bytecode");
    if (!bytecode || !cJSON_IsArray(bytecode)) {
        goto cleanup;
    }

    cJSON* buffer;
    cJSON_ArrayForEach(buffer, bytecode)
    {
        // Always use count as the write index
        int idx = ir_function->count;

        if (idx >= IR_FUNCTION_SIZE) {
            LOG_ERROR("IR function too large (max=%d), truncating", IR_FUNCTION_SIZE);
            goto cleanup;
        }

        IrInstruction* ir_instruction = ir_instruction_parse(buffer);
        if (!ir_instruction) {
            LOG_ERROR("Failed to parse IR instruction at index %d", idx);
            goto cleanup;
        }

        ir_instruction->seq = idx;
        ir_function->ir_instructions[idx] = ir_instruction;

        ir_function->count++;
    }

    return ir_function;

    cleanup:
        ir_function_delete(ir_function);
    return NULL;
}

IrFunction* ir_function_build(const Method* m, const Config* cfg)
{
    if (!m || !cfg) {
        return NULL;
    }

    char* source_decompiled = NULL;
    cJSON* source_json = NULL;
    IrFunction* ir_function = NULL;

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

    cJSON* method = opcode_get_method((Method*)m, methods);
    if (!method) {
        goto cleanup;
    }

    ir_function = parse_bytecode(method);

cleanup:
    if (source_json) {
        cJSON_Delete(source_json);
    }
    free(source_decompiled);

    return ir_function;
}

void ir_function_delete(IrFunction* ir_function)
{
    if (!ir_function)
        return;

    for (int i = 0; i < ir_function->count; i++) {
        IrInstruction* instr = ir_function->ir_instructions[i];
        if (instr)
            ir_instruction_delete(instr);
    }

    free(ir_function);
}
