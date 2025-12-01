#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

#include "opcode.h"
#include "log.h"

#include <stdlib.h>

static const char* opcode_signature[] = {
    [OP_LOAD] = "load",
    [OP_PUSH] = "push",
    [OP_BINARY] = "binary",
    [OP_GET] = "get",
    [OP_RETURN] = "return",
    [OP_IF_ZERO] = "ifz",
    [OP_IF] = "if",
    [OP_NEW] = "new",
    [OP_DUP] = "dup",
    [OP_INVOKE] = "invoke",
    [OP_THROW] = "throw",
    [OP_STORE] = "store",
    [OP_GOTO] = "goto",
    [OP_CAST] = "cast",
    [OP_NEW_ARRAY] = "newarray",
    [OP_ARRAY_LOAD] = "array_load",
    [OP_ARRAY_STORE] = "array_store",
    [OP_ARRAY_LENGTH] = "arraylength",
    [OP_INCR] = "incr",
    [OP_NEGATE] = "negate",
    [OP_COMPARE_FLOATING] = "comparefloating",
};

Opcode opcode_parse(cJSON* instruction_json)
{
    Opcode opcode = -1;

    cJSON* opr_object = cJSON_GetObjectItem(instruction_json, "opr");
    if (!opr_object || !cJSON_IsString(opr_object)) {
        LOG_ERROR("Opr is wrongly formatted or not exist");
        goto cleanup;
    }

    char* opr = cJSON_GetStringValue(opr_object);

    for (int i = 0; i < OP_COUNT; i++) {
        if (strcmp(opr, opcode_signature[i]) == 0) {
            opcode = i;
            goto cleanup;
        }
    }

cleanup:
    return opcode;
}

cJSON* opcode_get_method(Method* m, cJSON* methods)
{
    cJSON* buffer;
    cJSON_ArrayForEach(buffer, methods)
    {
        cJSON* method_name_obj = cJSON_GetObjectItem(buffer, "name");
        if (cJSON_IsString(method_name_obj)) {
            char* method_name = cJSON_GetStringValue(method_name_obj);
            if (strcmp(method_name, method_get_name(m)) == 0) {
                return buffer;
            }
        }
    }

    return NULL;
}

const char* opcode_print(Opcode opcode)
{
    if (opcode < 0 && opcode >= OP_COUNT) {
        return NULL;
    }

    return opcode_signature[opcode];
}


static void instruction_free(Instruction* inst) {
  if (!inst) return;

  switch (inst->opcode) {
  case OP_INVOKE: {
    InvokeOP* iv = &inst->data.invoke;

    free(iv->method_name);
    free(iv->ref_name);

    free(iv->args);

    break;
  }
  default:
    break;
  }

  free(inst);
}

void instruction_table_free(InstructionTable* table) {
  if (!table) return;

  for (int i = 0; i < table->count; i++) {
    instruction_free(table->instructions[i]);
  }

  free(table);
}