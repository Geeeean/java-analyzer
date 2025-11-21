#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "graph.h"
#include "ir_program.h"
#include "log.h"
#include "wpo.h"

struct AbstractContext {
    Cfg* cfg;
    WPO wpo;
    int block_count;
    int exit_count;
    int num_locals;
};

AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg)
{
    if (!m || !opts || !cfg) {
        goto cleanup;
    }

    IrFunction* ir_function = ir_program_get_function_ir(m, cfg);
    if (!ir_function) {
        goto cleanup;
    }

    Cfg* control_glow_graph = cfg_build(ir_function);
    if (!control_glow_graph) {
        goto cleanup;
    }

    Graph* graph = graph_from_cfg(control_glow_graph);
    if (!graph) {
        goto cleanup;
    }

    WPO wpo;
    Graph* test = graph_create_test_8_nodes();
    if (wpo_construct_aux(test, &wpo)) {
        // if (wpo_construct_aux(graph, &wpo)) {
        goto cleanup;
    }

    AbstractContext* ctx = malloc(sizeof(AbstractContext));
    if (!ctx) {
        goto cleanup;
    }

    ctx->block_count = vector_length(control_glow_graph->blocks);
    ctx->exit_count = vector_length(wpo.wpo->nodes) - ctx->block_count;
    ctx->wpo = wpo;
    ctx->cfg = control_glow_graph;
    ctx->num_locals = ir_program_get_num_locals(m, cfg);

#ifdef DEBUG
    graph_print(wpo.wpo);

    for (int i = 0; i < vector_length(ctx->wpo.Cx); i++) {
        LOG_DEBUG("COMPONENT %d, WITH HEAD: %d", i, *(int*)vector_get(ctx->wpo.heads, i));
        C* component = vector_get(ctx->wpo.Cx, i);
        for (int j = 0; j < vector_length(component->components); j++) {
            LOG_DEBUG("%d", *(int*)vector_get(component->components, j));
        }
    }
#endif

cleanup:
    LOG_ERROR("TODO cleanup in interpreter abstract setup");
    return ctx;
}

static void set_n_for_component(int exit_node, AbstractContext* ctx)
{
    int component_id = ctx->wpo.node_to_component[exit_node];
    C* component = vector_get(ctx->wpo.Cx, component_id);
    Vector* component_nodes = component->components;

    for (int i = 0; i < vector_length(component_nodes); i++) {
        // int 
    }
}

static void update_scheduling_successors(
    Graph* graph,
    int current_node,
    int* N,
    Vector* worklist,
    int* num_sched_pred)
{
    Node* node = vector_get(graph->nodes, current_node);
    for (int i = 0; i < vector_length(node->successors); i++) {
        int successor = *(int*)vector_get(node->successors, i);
        N[successor]++;

        if (num_sched_pred[successor] == N[successor]) {
            vector_push(worklist, &successor);
        }
    }
}

void apply_f(int current_node, AbstractContext* ctx)
{
    int component = ctx->wpo.node_to_component[current_node];
    int head = component == -1 ? -1 : *(int*)vector_get(ctx->wpo.heads, component);

    if (head == current_node) {
        // apply widening
    } else {
        // apply transfer
    }
}
int is_component_stabilized(int current_node) {
    return true;
}

void* interpreter_abstract_run(AbstractContext* ctx)
{
    int nodes_num = ctx->block_count + ctx->exit_count;
    if (!ctx) {
        goto cleanup;
    }

    if (!ctx->cfg) {
        goto cleanup;
    }

    int* N = calloc(nodes_num, sizeof(int));
    IntervalState** X = calloc(nodes_num, sizeof(IntervalState));

    if (!N || !X) {
        goto cleanup;
    }

#ifdef DEBUG
    for (int i = 0; i < nodes_num; i++) {
        LOG_DEBUG("NODE %d, COMPONENT %d", i, ctx->wpo.node_to_component[i]);
    }
#endif

    /*** INIT ***/
    int current_node = 0;
    X[current_node] = interval_new_top_state(ctx->num_locals);

    for (int i = current_node + 1; i < nodes_num; i++) {
        X[i] = interval_new_bottom_state(ctx->num_locals);
    }

    exit(1);

    Vector* worklist = vector_new(sizeof(int));
    vector_push(worklist, &current_node);

    while (!vector_pop(worklist, &current_node)) {
        LOG_DEBUG("NODE %d", current_node);

        N[current_node] = 0;

        /*** NonExit ***/
        if (current_node < ctx->block_count) {
            apply_f(current_node, ctx);

            // update successors
            update_scheduling_successors(ctx->wpo.wpo, current_node, N, worklist, ctx->wpo.num_sched_pred);
        }
        /*** Exit ***/
        else {
            if (is_component_stabilized(current_node)) {
                update_scheduling_successors(ctx->wpo.wpo, current_node, N, worklist, ctx->wpo.num_sched_pred);
            } else {
                set_n_for_component(current_node, ctx);
            }
        }
    }

cleanup:
    LOG_ERROR("TODO interpreter abstract run cleanup");
    return NULL;
}
