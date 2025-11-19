#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "graph.h"
#include "ir_program.h"
#include "log.h"
#include "scc.h"
#include "vector.h"
#include "wpo.h"


AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg)
{
    // IrFunction* ir_function = ir_program_get_function_ir(m, cfg);
    // Cfg* control_glow_graph = cfg_build(ir_function);
    // Graph* graph = graph_from_cfg(control_glow_graph);
    // WPO* wpo = wpo_construct(graph);

    Graph* graph = graph_create_test_8_nodes();
    WPO* wpo = wpo_construct_aux(graph);

    return NULL;
}
