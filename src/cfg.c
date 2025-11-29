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
    BasicBlock* b = malloc(sizeof(BasicBlock));
    if (!b) return NULL;

    b->id = id;
    b->og_id = id;
    b->successors = vector_new(sizeof(BasicBlock*));
    b->num_locals = 0;
    b->ir_function = NULL;
    b->ip_start = 0;
    b->ip_end = 0;

    return b;
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
    block_map = calloc(len, sizeof(BasicBlock*));
    if (!is_leader || !block_map) {
        result = FAILURE;
        goto cleanup;
    }

    // mark leaders
    is_leader[0] = 1;
    for (int i = 0; i < len; i++) {
        IrInstruction* ins = *(IrInstruction**)vector_get(ir_function->ir_instructions, i);
        switch (ins->opcode) {
            case OP_IF_ZERO:
            case OP_IF:
                is_leader[ins->data.ift.target] = 1;
                if (i + 1 < len) is_leader[i + 1] = 1;
                break;
            case OP_GOTO:
                is_leader[ins->data.go2.target] = 1;
                break;
            default:
                break;
        }
    }

    cfg = malloc(sizeof(Cfg));
    cfg->blocks = vector_new(sizeof(BasicBlock*));

    int id = 0;
    BasicBlock* last = NULL;

    // build blocks
    for (int i = 0; i < len; i++) {

        IrInstruction* ins = *(IrInstruction**)vector_get(ir_function->ir_instructions, i);
        Opcode op = ins->opcode;

        int is_term =
            op == OP_THROW ||
            op == OP_RETURN ||
            op == OP_GOTO ||
            op == OP_IF ||
            op == OP_IF_ZERO;

        if (is_leader[i]) {
            if (last) {
                last->ip_end = i - 1;
                vector_push(cfg->blocks, &last);
                block_map[last->ip_start] = last;
            }
            last = basic_block_new(id++);
            last->ip_start = i;
        }

        if (is_term) {
            if (!last) {
                last = basic_block_new(id++);
                last->ip_start = i;
            }
            last->ip_end = i;
            vector_push(cfg->blocks, &last);
            block_map[last->ip_start] = last;
            last = NULL;
        }
    }

    if (last) {
        last->ip_end = len - 1;
        vector_push(cfg->blocks, &last);
        block_map[last->ip_start] = last;
    }

    // successors
    for (int i = 0; i < vector_length(cfg->blocks); i++) {

        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        block->ir_function = ir_function;
        block->num_locals = num_locals;

        IrInstruction* ins = *(IrInstruction**)vector_get(
            ir_function->ir_instructions, block->ip_end);

        switch (ins->opcode) {
            case OP_RETURN:
            case OP_THROW:
                break;

            case OP_GOTO: {
                BasicBlock* tgt = block_map[ins->data.go2.target];
                vector_push(block->successors, &tgt);
                break;
            }

            case OP_IF:
            case OP_IF_ZERO: {
                BasicBlock* tgt = block_map[ins->data.ift.target];
                vector_push(block->successors, &tgt);

                if (i + 1 < vector_length(cfg->blocks)) {
                    BasicBlock* fall = *(BasicBlock**)vector_get(cfg->blocks, i + 1);
                    vector_push(block->successors, &fall);
                }
                break;
            }

            default:
                if (i + 1 < vector_length(cfg->blocks)) {
                    BasicBlock* fall = *(BasicBlock**)vector_get(cfg->blocks, i + 1);
                    vector_push(block->successors, &fall);
                }
        }
    }

cleanup:
    free(is_leader);
    free(block_map);
    free(visited);

    return cfg;
}

int cfg_inline(Cfg* cfg, Config* config, Method* m)
{
    if (!cfg) return FAILURE;

    IrFunction* ir_function = ir_program_get_function_ir(m, config);
    BasicBlock* entry = *(BasicBlock**)vector_get(cfg->blocks, 0);

    int length = vector_length(cfg->blocks);

    for (int i = 0; i < length; i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);

        for (int j = block->ip_start; j <= block->ip_end; j++) {

            IrInstruction* ins =
                *(IrInstruction**)vector_get(ir_function->ir_instructions, j);

            if (ins->opcode != OP_INVOKE) continue;

            // split block
            int new_id = vector_length(cfg->blocks);

            BasicBlock* invoke_exit = basic_block_new(new_id);
            invoke_exit->ip_start = j + 1;
            invoke_exit->ip_end = block->ip_end;
            invoke_exit->ir_function = block->ir_function;

            // copy successors manually
            for (int s = 0; s < vector_length(block->successors); s++) {
                BasicBlock* succ = *(BasicBlock**)vector_get(block->successors, s);
                vector_push(invoke_exit->successors, &succ);
            }

            vector_delete(block->successors);
            block->successors = vector_new(sizeof(BasicBlock*));

            // resolve invoked method
            InvokeOP* inv = &ins->data.invoke;

            char* method_id = get_method_signature(inv);
            if (!method_id) return FAILURE;

            char* method_id_copy = strdup(method_id);  // strtok needs writable
            char* root = strtok(method_id_copy, ".");

            if (strcmp(root, "jpamb") != 0) {
                free(method_id_copy);
                free(method_id);
                continue;
            }

            free(method_id_copy);

            Method* invoked = method_create(method_id);
            free(method_id);

            // detect recursive invoke
            if (strcmp(method_get_id(invoked), method_get_id(m)) == 0) {
                vector_push(block->successors, &entry);
                continue;
            }

            Cfg* invoked_cfg = ir_program_get_cfg(invoked, config);
            cfg_inline(invoked_cfg, config, invoked);

            // insert split block
            vector_push(cfg->blocks, &invoke_exit);

            int base = vector_length(cfg->blocks);

            // insert invoked cfg blocks
            for (int k = 0; k < vector_length(invoked_cfg->blocks); k++) {

                BasicBlock* b2 =
                    *(BasicBlock**)vector_get(invoked_cfg->blocks, k);

                b2->id += base;
                vector_push(cfg->blocks, &b2);

                IrInstruction* last =
                    *(IrInstruction**)vector_get(
                        b2->ir_function->ir_instructions, b2->ip_end);

                if (last->opcode == OP_RETURN)
                    vector_push(b2->successors, &invoke_exit);
            }

            // entry edge into invoked cfg
            BasicBlock* entry2 = *(BasicBlock**)vector_get(invoked_cfg->blocks, 0);
            vector_push(block->successors, &entry2);

            block->ip_end = j;
        }
    }

    return SUCCESS;
}

void cfg_print(Cfg* cfg)
{
    for (int i = 0; i < vector_length(cfg->blocks); i++) {

        BasicBlock* b = *(BasicBlock**)vector_get(cfg->blocks, i);

        LOG_INFO("BLOCK %d (OG %d) [%d-%d] locals:%d",
                 b->id, b->og_id, b->ip_start, b->ip_end, b->num_locals);

        for (int s = 0; s < vector_length(b->successors); s++) {
            BasicBlock* succ = *(BasicBlock**)vector_get(b->successors, s);
            LOG_INFO("  -> %d", succ->id);
        }
    }
}

void cfg_delete(Cfg* cfg)
{
    if (!cfg) return;

    if (cfg->blocks) {
        for (int i = 0; i < vector_length(cfg->blocks); i++) {
            BasicBlock* b = *(BasicBlock**)vector_get(cfg->blocks, i);
            if (b->successors)
                vector_delete(b->successors);
            free(b);
        }
        vector_delete(cfg->blocks);
    }

    free(cfg);
}
