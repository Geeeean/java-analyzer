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
}

AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg)
{
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
