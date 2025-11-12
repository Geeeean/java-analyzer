#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "ir_program.h"
#include "log.h"

struct AbstractContext {
    IrFunction* ir_function;
    Cfg* cfg;
    Vector* in; // Vector of IntervalState*
    Vector* out; // Vector of IntervalState*
    int num_locals;
};

static void interpreter_abstract_print_result(const AbstractContext* ctx)
{
    if (!ctx || !ctx->cfg || !ctx->cfg->blocks)
        return;

    int num_blocks = vector_length(ctx->cfg->blocks);

    LOG_INFO("RESULT ABSTRACT ANALYSIS");

    for (int i = 0; i < num_blocks; i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(ctx->cfg->blocks, i);
        IntervalState* in_state = *(IntervalState**)vector_get(ctx->in, i);
        IntervalState* out_state = *(IntervalState**)vector_get(ctx->out, i);

        LOG_INFO("Block %d: [%d, %d]", block->id, block->ip_start, block->ip_end);
        LOG_INFO("IN : ");
        interval_state_print(in_state);
        LOG_INFO("OUT: ");
        interval_state_print(out_state);
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

    int num_block = vector_length(cfg->blocks);

    // push blocks in rpo order
    Vector* worklist = vector_new(sizeof(int));
    for (int i = 0; i < vector_length(cfg->rpo); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->rpo, i);
        vector_push(worklist, &block->id);
    }

    int block_index;
    while (!vector_pop(worklist, &block_index)) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, block_index);
        IntervalState* in_state = *(IntervalState**)vector_get(abstract_context->in, block_index);
        IntervalState* out_state = *(IntervalState**)vector_get(abstract_context->out, block_index);

        // OUT[b] = transfer(IN[b])
        interval_state_copy(out_state, in_state);
        for (int i = block->ip_start; i <= block->ip_end; i++) {
            IrInstruction* ir_instruction = *(IrInstruction**)vector_get(abstract_context->ir_function->ir_instructions, i);
            interval_transfer(out_state, ir_instruction);
        }

        // IN[s] = join(IN[s], OUT[b])
        for (int i = 0; i < vector_length(block->successors); i++) {
            BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, i);
            int id = successor->id;
            IntervalState* successor_in_state = *(IntervalState**)vector_get(abstract_context->in, id);

            int changed;
            interval_join(successor_in_state, out_state, &changed);

            if (changed) {
                vector_push(worklist, &id);
            }
        }
    }

    interpreter_abstract_print_result(abstract_context);
}
