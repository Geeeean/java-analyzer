#include "ir_function.h"

#include "cJSON/cJSON.h"
#include "ir_instruction.h"
#include "log.h"
#include "method.h"

static IrFunction* parse_bytecode(cJSON* method_json)
{
    if (!method_json)
        return NULL;

    IrFunction* ir = calloc(1, sizeof(IrFunction));
    if (!ir)
        return NULL;

    ir->capacity = IR_FUNCTION_SIZE;
    ir->count    = 0;

    cJSON* code = cJSON_GetObjectItem(method_json, "code");
    if (!code)
        goto fail;

    cJSON* bytecode = cJSON_GetObjectItem(code, "bytecode");
    if (!bytecode || !cJSON_IsArray(bytecode))
        goto fail;

    int idx = 0;
    cJSON* buffer;

    cJSON_ArrayForEach(buffer, bytecode)
    {
        if (idx >= IR_FUNCTION_SIZE) {
            LOG_ERROR("IR function too large (max=%d), truncating", IR_FUNCTION_SIZE);
            goto fail;
        }

        IrInstruction* ins = ir_instruction_parse(buffer);
        if (!ins) {
            LOG_ERROR("Failed to parse IR instruction at index %d", idx);
            goto fail;
        }

        ins->seq = idx;
        ir->ir_instructions[idx] = ins;

        idx++;
        ir->count = idx;
    }

    return ir;

fail:
    ir_function_delete(ir);
    return NULL;
}

IrFunction* ir_function_build(const Method* m, const Config* cfg)
{
    if (!m || !cfg)
        return NULL;

    char* source = method_read(m, cfg, SRC_DECOMPILED);
    if (!source)
        return NULL;

    cJSON* json = cJSON_Parse(source);
    free(source);

    if (!json)
        return NULL;

    cJSON* methods = cJSON_GetObjectItem(json, "methods");
    if (!methods || !cJSON_IsArray(methods)) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON* method_json = opcode_get_method((Method*)m, methods);
    if (!method_json) {
        cJSON_Delete(json);
        return NULL;
    }

    IrFunction* ir = parse_bytecode(method_json);

    cJSON_Delete(json);
    return ir;
}

void ir_function_delete(IrFunction* ir)
{
    if (!ir)
        return;

    for (int i = 0; i < ir->count; i++) {
        IrInstruction* ins = ir->ir_instructions[i];
        if (ins)
            ir_instruction_delete(ins);
    }

    free(ir);
}
