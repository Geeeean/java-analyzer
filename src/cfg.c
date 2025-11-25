#include "cfg.h"
#include "common.h"
#include "ir_instruction.h"
#include "ir_program.h"
#include "log.h"
#include "stdint.h"
#include "utils.h"
#include "vector.h"

#include <string.h>

static BasicBlock* basic_block_new(int id)
{
    BasicBlock* basic_block = malloc(sizeof(BasicBlock));
    if (!basic_block) {
        return NULL;
    }

    basic_block->id = id;
    basic_block->og_id = id;
    basic_block->successors = vector_new(sizeof(BasicBlock*));

    return basic_block;
}

Cfg* cfg_build(IrFunction* ir_function, int num_locals)
{
    int result = SUCCESS;
    Cfg* cfg = NULL;
    int8_t* is_leader = NULL;
    BasicBlock** block_map = NULL;
    int8_t* visited = NULL;

    if (!ir_function) {
        result = FAILURE;
        goto cleanup;
    }

    int len = vector_length(ir_function->ir_instructions);

    is_leader = calloc(len, sizeof(int8_t));
    if (!is_leader) {
        result = FAILURE;
        goto cleanup;
    }

    block_map = calloc(len, sizeof(BasicBlock*));
    if (!block_map) {
        result = FAILURE;
        goto cleanup;
    }

    // leaders assignment
    is_leader[0] = 1;
    for (int i = 0; i < len; i++) {
        IrInstruction* ir_instruction = *(IrInstruction**)vector_get(ir_function->ir_instructions, i);
        if (!ir_instruction) {
            result = FAILURE;
            goto cleanup;
        }

        switch (ir_instruction->opcode) {
        case OP_IF_ZERO:
        case OP_IF: {
            int target = ir_instruction->data.ift.target;
            is_leader[target] = 1;
            if (i + 1 < len) {
                is_leader[i + 1] = 1;
            }
            break;
        };
        case OP_GOTO: {
            int target = ir_instruction->data.go2.target;
            is_leader[target] = 1;
            break;
        };
        default:
            break;
        }
    }

    cfg = malloc(sizeof(Cfg));
    if (!cfg) {
        result = FAILURE;
        goto cleanup;
    }

    cfg->blocks = vector_new(sizeof(BasicBlock*));
    if (!cfg->blocks) {
        result = FAILURE;
        goto cleanup;
    }

    int id = 0;
    BasicBlock* last = NULL;

    // basic blocks build
    for (int i = 0; i < len; i++) {
        IrInstruction* ir_instruction = *(IrInstruction**)vector_get(ir_function->ir_instructions, i);
        if (!ir_instruction) {
            result = FAILURE;
            goto cleanup;
        }

        Opcode opcode = ir_instruction->opcode;

        int8_t is_terminator = opcode == OP_THROW
            || opcode == OP_RETURN
            || opcode == OP_GOTO
            || opcode == OP_IF
            || opcode == OP_IF_ZERO;

        // start a new block
        if (is_leader[i]) {
            if (last) {
                last->ip_end = i - 1;
                vector_push(cfg->blocks, &last);
                block_map[last->ip_start] = last;
                last = NULL;
            }

            last = basic_block_new(id);
            if (!last) {
                result = FAILURE;
                goto cleanup;
            }

            last->ip_start = i;
            id++;
        }

        // close the block here
        if (is_terminator) {
            if (!last) {
                last = basic_block_new(id);
                if (!last) {
                    result = FAILURE;
                    goto cleanup;
                }

                last->ip_start = i;
                id++;
            }

            last->ip_end = i;

            vector_push(cfg->blocks, &last);
            block_map[last->ip_start] = last;
            last = NULL;
        }
    }

    if (last) {
        last->ip_end = len - 1;
        block_map[last->ip_start] = last;
        vector_push(cfg->blocks, &last);

        last = NULL;
    }

    // assign successors
    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        if (!block) {
            result = FAILURE;
            goto cleanup;
        }

        IrInstruction* ir_instruction = *(IrInstruction**)vector_get(ir_function->ir_instructions, block->ip_end);
        if (!ir_instruction) {
            result = FAILURE;
            goto cleanup;
        }

        switch (ir_instruction->opcode) {
        case OP_RETURN:
        case OP_THROW:
            break;
        case OP_GOTO: {
            BasicBlock* target_block = block_map[ir_instruction->data.go2.target];
            if (!target_block) {
                result = FAILURE;
                goto cleanup;
            }
            vector_push(block->successors, &target_block);
            break;
        }
        case OP_IF:
        case OP_IF_ZERO: {
            BasicBlock* target_block = block_map[ir_instruction->data.ift.target];
            if (!target_block) {
                result = FAILURE;
                goto cleanup;
            }
            vector_push(block->successors, &target_block);

            if (i + 1 < vector_length(cfg->blocks)) {
                BasicBlock* next_block = *(BasicBlock**)vector_get(cfg->blocks, i + 1);
                vector_push(block->successors, &next_block);
            }
            break;
        }
        default: {
            if (i + 1 < vector_length(cfg->blocks)) {
                BasicBlock* next_block = *(BasicBlock**)vector_get(cfg->blocks, i + 1);
                vector_push(block->successors, &next_block);
            }
            break;
        }
        }
    }

    visited = calloc(vector_length(cfg->blocks), sizeof(int8_t));
    if (!visited) {
        result = FAILURE;
        goto cleanup;
    }

    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        block->ir_function = ir_function;
        block->num_locals = num_locals;
    }

cleanup:
    free(visited);
    free(block_map);
    free(is_leader);
    if (result) {
        vector_delete(cfg->blocks);
        free(cfg);
        cfg = NULL;
    }

    return cfg;
}

int cfg_inline(Cfg* cfg, Config* config, Method* m)
{
    if (!cfg) {
        return FAILURE;
    }

    IrFunction* ir_function = ir_program_get_function_ir(m, config);
    BasicBlock* entry = *(BasicBlock**)vector_get(cfg->blocks, 0);

    int length = vector_length(cfg->blocks);
    for (int i = 0; i < length; i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        for (int j = block->ip_start; j <= block->ip_end; j++) {
            IrInstruction* ir_instruction = *(IrInstruction**)vector_get(ir_function->ir_instructions, j);
            if (ir_instruction->opcode == OP_INVOKE) {
                int id = vector_length(cfg->blocks);
                // split the current block
                BasicBlock* invoke_exit = basic_block_new(id);
                invoke_exit->ip_start = j + 1;
                invoke_exit->ip_end = block->ip_end;
                invoke_exit->ir_function = block->ir_function;
                vector_copy(invoke_exit->successors, block->successors);
                vector_delete(block->successors);

                block->successors = vector_new(sizeof(BasicBlock*));
                // vector_push(block->successors, &invoke_exit);

                InvokeOP* invoke = &ir_instruction->data.invoke;
                char* method_id = get_method_signature(invoke);
                if (!method_id) {
                    return FAILURE;
                }

                char* method_id_copy = strdup(method_id);
                char* root = strtok(method_id_copy, ".");

                if (strcmp(root, "jpamb") != 0) {
                    continue;
                }

                free(method_id_copy);

                Method* invoke_method = method_create(method_id);
                // recursive
                if (strcmp(method_get_id(invoke_method), method_get_id(m)) == 0) {
                    vector_push(block->successors, &entry);
                    exit(1);
                } else {
                    Cfg* invoke_cfg = ir_program_get_cfg(invoke_method, config);
                    cfg_inline(invoke_cfg, config, invoke_method);

                    IrFunction* invoke_function = ir_program_get_function_ir(invoke_method, config);

                    vector_push(cfg->blocks, &invoke_exit);
                    int actual_len = vector_length(cfg->blocks);
                    for (int k = 0; k < vector_length(invoke_cfg->blocks); k++) {
                        BasicBlock* invoke_cfg_block = *(BasicBlock**)vector_get(invoke_cfg->blocks, k);
                        invoke_cfg_block->id = invoke_cfg_block->id + actual_len;

                        vector_push(cfg->blocks, &invoke_cfg_block);

                        IrInstruction* last = *(IrInstruction**)vector_get(invoke_function->ir_instructions, invoke_cfg_block->ip_end);
                        if (last->opcode == OP_RETURN) {
                            vector_push(invoke_cfg_block->successors, &invoke_exit);
                        }
                    }

                    BasicBlock* invoke_cfg_entry = *(BasicBlock**)vector_get(invoke_cfg->blocks, 0);
                    vector_push(block->successors, &invoke_cfg_entry);
                }

                block->ip_end = j;
            }
        }
    }

    return SUCCESS;
}

void cfg_print(Cfg* cfg)
{
    LOG_DEBUG("PRINT");
    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        LOG_INFO("BLOCK %d (OG ID: %d), [%d-%d] ir_function: %p", block->id, block->og_id, block->ip_start, block->ip_end, block->ir_function);

        for (int j = 0; j < vector_length(block->successors); j++) {
            BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, j);
            LOG_INFO("%2d ", successor->id);
        }
        printf("\n");
    }
}
