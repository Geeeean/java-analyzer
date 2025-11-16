#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "ir_program.h"
#include "log.h"
#include "vector.h"

struct AbstractContext {
    IrFunction* ir_function;
    Cfg* cfg;
    Vector* in; // Vector of IntervalState*
    Vector* out; // Vector of IntervalState*
    int num_locals;
};

static void interpreter_abstract_print_result(const AbstractContext* ctx, int8_t* bottom)
{
    if (!ctx || !ctx->cfg || !ctx->cfg->blocks)
        return;

    int num_blocks = vector_length(ctx->cfg->blocks);

    LOG_INFO("RESULT ABSTRACT ANALYSIS");

    for (int i = 0; i < num_blocks; i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(ctx->cfg->blocks, i);
        IntervalState* in_state = *(IntervalState**)vector_get(ctx->in, i);
        IntervalState* out_state = *(IntervalState**)vector_get(ctx->out, i);

        LOG_INFO("Block %d: [%d, %d] (success len: %ld)", block->id, block->ip_start, block->ip_end, vector_length(block->successors));
#ifdef DEBUG
        LOG_INFO("IN : ");
        interval_state_print(in_state);
        LOG_INFO("OUT: ");
#endif
        if (bottom[i]) {
            LOG_INFO("⊥");
        } else {
            interval_state_print(out_state);
        }
        LOG_INFO("");
    }
}

AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg)
{
    Cfg* control_flow = ir_program_get_cfg(m, cfg);
    IrFunction* ir_function = ir_program_get_function_ir(m, cfg);
    int num_locals = ir_program_get_num_locals(m, cfg);

    if (!cfg) {
        LOG_ERROR("Config is null");
        return NULL;
    }

    if (!control_flow) {
        LOG_ERROR("control_flow is null");
        return NULL;
    }

    if (!control_flow->blocks) {
        LOG_ERROR("control_flow->blocks is null");
        return NULL;
    }

    if (num_locals < 0) {
        LOG_ERROR("num_locals is less than 0");
        return NULL;
    }

    if (!ir_function) {
        LOG_ERROR("ir_function is null");
        return NULL;
    }

    int len = vector_length(control_flow->blocks);

    Vector* in = vector_new(sizeof(IntervalState*));
    Vector* out = vector_new(sizeof(IntervalState*));
    if (!in || !out) {
        return NULL;
    }

    for (int i = 0; i < len; i++) {
        IntervalState* st = interval_new_top_state(num_locals);
        vector_push(in, &st);

        IntervalState* st2 = interval_new_top_state(num_locals);
        vector_push(out, &st2);
    }

    AbstractContext* abstract_context = malloc(sizeof(AbstractContext));
    if (!abstract_context) {
        return NULL;
    }

    abstract_context->cfg = control_flow;
    abstract_context->in = in;
    abstract_context->out = out;
    abstract_context->num_locals = num_locals;
    abstract_context->ir_function = ir_function;

    return abstract_context;
}

static void intersect(AbstractContext* abstract_context, BasicBlock* block, IntervalState* state, Vector* worklist, int index)
{
    BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, index);
    int id = successor->id;
    IntervalState* successor_in_state = *(IntervalState**)vector_get(abstract_context->in, id);

    int changed;
    interval_intersection(successor_in_state, state, &changed);

    if (changed) {
        vector_push(worklist, &id);
    }
}

static void join(AbstractContext* abstract_context, BasicBlock* block, IntervalState* state, Vector* worklist, int index)
{
    BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, index);
    int id = successor->id;
    IntervalState* successor_in_state = *(IntervalState**)vector_get(abstract_context->in, id);

    int changed;
    interval_join(successor_in_state, state, &changed);

    if (changed) {
        vector_push(worklist, &id);
    }
}

void interpreter_abstract_run(AbstractContext* abstract_context)
{
    if (!abstract_context
        || !abstract_context->cfg
        || !abstract_context->cfg->blocks
        || !abstract_context->in
        || !abstract_context->out) {
        return;
    }

    Cfg* cfg = abstract_context->cfg;
    int num_blocks = vector_length(cfg->blocks);
    int num_locals = abstract_context->num_locals;

    int8_t* visited = calloc(num_blocks, sizeof(int8_t));
    int8_t* bottom = calloc(num_blocks, sizeof(int8_t));
    if (!visited || !bottom) {
        return;
    }

    Vector* worklist = vector_new(sizeof(int));
    if (!worklist) {
        free(visited);
        return;
    }

    int id = 0;
    vector_push(worklist, &id);

    visited[id] = 1;
    while (!vector_pop(worklist, &id)) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, id);
        if (!block) {
            continue;
        }

        IntervalState* in_state = *(IntervalState**)vector_get(abstract_context->in, id);
        IntervalState* out_state = *(IntervalState**)vector_get(abstract_context->out, id);

        // ----------------------------------------------------
        // OUT[block] = transfer(IN[block], instruction)
        // ----------------------------------------------------
        interval_state_copy(out_state, in_state);

        // all instruction - last
        for (int ip = block->ip_start; ip < block->ip_end; ip++) {
            IrInstruction* ir = *(IrInstruction**)vector_get(
                abstract_context->ir_function->ir_instructions, ip);
            interval_transfer(out_state, ir);
        }

        // last instr
        IrInstruction* last_ir = *(IrInstruction**)vector_get(
            abstract_context->ir_function->ir_instructions, block->ip_end);

        if (ir_instruction_is_conditional(last_ir)) {
            // ---------------------------------------------
            // cond branch
            // ---------------------------------------------
            IntervalState* out_true = interval_new_top_state(num_locals);
            IntervalState* out_false = interval_new_top_state(num_locals);

            interval_state_copy(out_true, out_state);
            interval_state_copy(out_false, out_state);

            interval_transfer_conditional(out_true, out_false, last_ir);

            if (vector_length(block->successors) != 2) {
                LOG_ERROR("Conditional block %d without 2 successors", block->id);
            } else {
                BasicBlock* succ_true = *(BasicBlock**)vector_get(block->successors, 0);
                BasicBlock* succ_false = *(BasicBlock**)vector_get(block->successors, 1);

                if (succ_true) {
                    if (is_interval_state_bottom(out_true)) {
                        bottom[id] = true;
                    } else {
                        IntervalState* succ_in = *(IntervalState**)vector_get(
                            abstract_context->in, succ_true->id);

                        int changed = 0;
                        if (!visited[succ_true->id]) {
                            interval_state_copy(succ_in, out_true);
                            visited[succ_true->id] = true;
                            changed = true;
                        } else {
                            interval_join(succ_in, out_true, &changed);
                        }

                        if (changed) {
                            int sid = succ_true->id;
                            vector_push(worklist, &sid);
                        }
                    }
                }

                if (succ_false) {
                    if (is_interval_state_bottom(out_false)) {
                        bottom[id] = true;
                    } else {
                        IntervalState* succ_in = *(IntervalState**)vector_get(
                            abstract_context->in, succ_false->id);

                        int changed = 0;
                        if (!visited[succ_false->id]) {
                            interval_state_copy(succ_in, out_false);
                            visited[succ_false->id] = true;
                            changed = true;
                        } else {
                            interval_join(succ_in, out_false, &changed);
                        }

                        if (changed) {
                            int sid = succ_false->id;
                            vector_push(worklist, &sid);
                        }
                    }
                }
            }
        } else {
            interval_transfer(out_state, last_ir);
            if (is_interval_state_bottom(out_state)) {
                bottom[id] = true;
                continue;
            }

            for (int i = 0; i < vector_length(block->successors); i++) {
                BasicBlock* succ = *(BasicBlock**)vector_get(block->successors, i);
                if (!succ)
                    continue;

                IntervalState* succ_in = *(IntervalState**)vector_get(
                    abstract_context->in, succ->id);

                int changed = 0;
                if (!visited[succ->id]) {
                    // prima volta: niente join con TOP, copiamo
                    interval_state_copy(succ_in, out_state);
                    visited[succ->id] = 1;
                    changed = 1;
                } else {
                    interval_join(succ_in, out_state, &changed);
                }

                if (changed) {
                    int sid = succ->id;
                    vector_push(worklist, &sid);
                }
            }
        }

#ifdef DEBUG
        interpreter_abstract_print_result(abstract_context);
#endif
    }

    for (int i = 0; i < num_blocks; i++) {
        if (!visited[i]) {
            bottom[i] = true;
        }
    }

    interpreter_abstract_print_result(abstract_context, bottom);
    free(bottom);
    free(visited);
}
