#include "ir_function.h"
#include "cJSON/cJSON.h"
#include "ir_instruction.h"
#include "log.h"
#include "method.h"

static IrFunction*
parse_bytecode(cJSON* method)
{
    IrFunction* ir_function = calloc(1, sizeof(*ir_function));
    if (!ir_function) {
        goto cleanup;
    }

    ir_function->ir_instructions = vector_new(sizeof(IrInstruction*));
    if (!ir_function->ir_instructions) {
        goto cleanup;
    }

    cJSON* code = cJSON_GetObjectItem(method, "code");
    if (!code) {
        goto cleanup;
    }

    cJSON* bytecode = cJSON_GetObjectItem(code, "bytecode");
    if (!bytecode || !cJSON_IsArray(bytecode)) {
        goto cleanup;
    }

    cJSON* buffer;
    int i = 0;
    cJSON_ArrayForEach(buffer, bytecode)
    {
        IrInstruction* ir_instruction = ir_instruction_parse(buffer);
        ir_instruction->seq = i;
        if (!ir_instruction) {
            goto cleanup;
        }

        vector_push(ir_function->ir_instructions, &ir_instruction);
        ir_instruction = *(IrInstruction**)vector_get(ir_function->ir_instructions, i);

        i++;
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
    if (ir_function) {
        vector_delete(ir_function->ir_instructions);
        free(ir_function);
    }
}
