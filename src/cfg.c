#include "cfg.h"
#include "common.h"
#include "ir_instruction.h"
#include "stdint.h"
#include "vector.h"

typedef struct {
    int id;
    int ip_start;
    int ip_end;
    Vector* successors;
} BasicBlock;

struct Cfg {
    Vector* blocks;
    Vector* rpo;
};

static BasicBlock* basic_block_new(int id)
{
    BasicBlock* basic_block = malloc(sizeof(BasicBlock));
    if (!basic_block) {
        return NULL;
    }

    basic_block->id = id;
    basic_block->successors = vector_new(sizeof(BasicBlock*));

    return basic_block;
}

static void compute_rpo(BasicBlock* block, int8_t* visited, Vector* rpo)
{
    if (block && !visited[block->id]) {
        visited[block->id] = 1;

        for (int i = 0; i < vector_length(block->successors); i++) {
            BasicBlock* next = vector_get(block->successors, i);
            if (next && !visited[next->id]) {
                compute_rpo(next, visited, rpo);
            }
        }

        vector_push(rpo, &block);
    }
}

Cfg* cfg_build(IrFunction* ir_function)
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

    is_leader = calloc(ir_function->count, sizeof(int8_t));
    if (!is_leader) {
        result = FAILURE;
        goto cleanup;
    }

    block_map = calloc(ir_function->count, sizeof(BasicBlock*));
    if (!block_map) {
        result = FAILURE;
        goto cleanup;
    }

    // leaders assignment
    is_leader[0] = 1;
    for (int i = 0; i < ir_function->count; i++) {
        IrInstruction* ir_instruction = ir_function->ir_instructions[i];

        switch (ir_instruction->opcode) {
        case OP_IF_ZERO:
        case OP_IF: {
            int target = ir_instruction->data.ift.target;
            is_leader[target] = 1;
            if (i + 1 < ir_function->count) {
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
    for (int i = 0; i < ir_function->count; i++) {
        IrInstruction* ir_instruction = ir_function->ir_instructions[i];
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
        last->ip_end = ir_function->count - 1;
        block_map[last->ip_start] = last;
        vector_push(cfg->blocks, &last);

        last = NULL;
    }

    // assign successors
    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = vector_get(cfg->blocks, i);
        if (!block) {
            result = FAILURE;
            goto cleanup;
        }

        IrInstruction* ir_instruction = ir_function->ir_instructions[block->ip_end];
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
                BasicBlock* next_block = vector_get(cfg->blocks, i + 1);
                vector_push(block->successors, &next_block);
            }
            break;
        }
        default: {
            if (i + 1 < vector_length(cfg->blocks)) {
                BasicBlock* next_block = vector_get(cfg->blocks, i + 1);
                vector_push(block->successors, &next_block);
            }
            break;
        }
        }
    }

    Vector* rpo = vector_new(sizeof(BasicBlock*));
    if (!rpo) {
        result = FAILURE;
        goto cleanup;
    }

    visited = calloc(vector_length(cfg->blocks), sizeof(int8_t));
    if (!visited) {
        result = FAILURE;
        goto cleanup;
    }

    compute_rpo(vector_get(cfg->blocks, 0), visited, rpo);
    vector_reverse(rpo);

    cfg->rpo = rpo;

cleanup:
    free(visited);
    free(block_map);
    free(is_leader);
    if (result) {
        free(cfg);
        cfg = NULL;
    }

    return cfg;
}
