#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "ir_program.h"
#include "log.h"
#include "scc.h"
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
}

void print_cfg(Cfg* cfg)
{
    printf("=== CFG ===\n");

    int block_count = vector_length(cfg->blocks);

    for (int i = 0; i < block_count; i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);

        printf("Blocco [%d-%d] %d:\n", block->ip_start, block->ip_end, block->id);

        for (int j = 0; j < vector_length(block->successors); j++) {
            BasicBlock* succ = *(BasicBlock**)vector_get(block->successors, j);
            printf("  -> %d\n", succ->id);
        }

        printf("\n");
    }
}

AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg)
{
    Cfg* control_flow_graph = ir_program_get_cfg(m, cfg);
    print_cfg(control_flow_graph);
    cscc_build(control_flow_graph);
    return NULL;
}

static void intersect(AbstractContext* abstract_context, BasicBlock* block, IntervalState* state, Vector* worklist, int index)
{
}

static void join(AbstractContext* abstract_context, BasicBlock* block, IntervalState* state, Vector* worklist, int index)
{
}

void interpreter_abstract_run(AbstractContext* abstract_context)
{
}
