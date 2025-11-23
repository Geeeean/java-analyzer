#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "graph.h"
#include "ir_program.h"
#include "log.h"
#include "wpo.h"

struct AbstractContext {
    Cfg* cfg;
    IrFunction* ir_function;
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

    Cfg* control_flow_graph = ir_program_get_cfg(m, cfg);
#ifdef DEBUG
    cfg_print(control_flow_graph);
#endif
    if (!control_flow_graph) {
        goto cleanup;
    }

    Graph* graph = graph_from_cfg(control_flow_graph);
    if (!graph) {
        goto cleanup;
    }

    WPO wpo;
    // Graph* test = graph_create_test_8_nodes();
    // if (wpo_construct_aux(test, &wpo)) {
    if (wpo_construct_aux(graph, &wpo)) {
        goto cleanup;
    }

    AbstractContext* ctx = malloc(sizeof(AbstractContext));
    if (!ctx) {
        goto cleanup;
    }

    ctx->block_count = vector_length(control_flow_graph->blocks);
    ctx->exit_count = vector_length(wpo.wpo->nodes) - ctx->block_count;
    ctx->wpo = wpo;
    ctx->cfg = control_flow_graph;
    ctx->num_locals = ir_program_get_num_locals(m, cfg);
    ctx->ir_function = ir_function;

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

static void set_n_for_component(int* N, int exit_node, AbstractContext* ctx, Vector* worklist)
{
    int component_id = ctx->wpo.node_to_component[exit_node];
    C* component = vector_get(ctx->wpo.Cx, component_id);
    Vector* component_nodes = component->components;

    for (int i = 0; i < vector_length(component_nodes); i++) {
        int node = *(int*)vector_get(component_nodes, i);
        N[node] = ctx->wpo.num_outer_sched_pred[component_id][node];

        if (ctx->wpo.num_sched_pred[node] == N[node]) {
            vector_push(worklist, &node);
        }
    }
}

static void update_scheduling_successors(
    Graph* graph,
    int current_node,
    int* N,
    Vector* worklist,
    int* num_sched_pred,
    IntervalState** X_in,
    IntervalState** X_out,
    int is_conditional)
{
    Node* node = vector_get(graph->nodes, current_node);
    for (int i = 0; i < vector_length(node->successors); i++) {
        int successor = *(int*)vector_get(node->successors, i);
        N[successor]++;

        if (!(is_conditional && (i == 0 || i == 1))) {
            int dummy;
            interval_join(X_in[successor], X_out[current_node], &dummy);
        }

        if (num_sched_pred[successor] == N[successor]) {
            vector_push(worklist, &successor);
        }
    }
}

int apply_f(int current_node, AbstractContext* ctx, IntervalState** X_in, IntervalState** X_out, int* changed)
{
    int component = ctx->wpo.node_to_component[current_node];
    int head = component == -1 ? -1 : *(int*)vector_get(ctx->wpo.heads, component);

    IntervalState* in = X_in[current_node];
    IntervalState* out = X_out[current_node];

    interval_state_copy(out, in);

    BasicBlock* block = *(BasicBlock**)vector_get(ctx->cfg->blocks, current_node);

    for (int ip = block->ip_start; ip < block->ip_end; ip++) {
        IrInstruction* ir = *(IrInstruction**)
                                vector_get(ctx->ir_function->ir_instructions, ip);
        interval_transfer(out, ir);
    }

    IrInstruction* last = *(IrInstruction**)vector_get(ctx->ir_function->ir_instructions,
        block->ip_end);

    if (ir_instruction_is_conditional(last)) {
        IntervalState* out_true = interval_new_top_state(ctx->num_locals);
        IntervalState* out_false = interval_new_top_state(ctx->num_locals);

        interval_state_copy(out_true, out);
        interval_state_copy(out_false, out);

        interval_transfer_conditional(out_true, out_false, last);

        Node* node = vector_get(ctx->wpo.wpo->nodes, current_node);

        int dummy;
        int* successor_true = vector_get(node->successors, 0);
        if (successor_true) {
            interval_join(X_in[*successor_true], out_true, &dummy);
        }

        int* successor_false = vector_get(node->successors, 1);
        if (successor_false) {
            interval_join(X_in[*successor_false], out_false, &dummy);
        }
    } else {
        interval_transfer(out, last);
    }

    if (head == current_node) {
        // if (ir_instruction_is_conditional(last)) {
        //     LOG_ERROR("OK THIS CAN HAPPEN");
        // }

        interval_widening(in, out, changed);
    } else {
        interval_join(in, out, changed);
    }

    return ir_instruction_is_conditional(last);
}

int is_component_stabilized(int current_node, AbstractContext* ctx, IntervalState** X_in, IntervalState** X_out)
{
    int component_id = ctx->wpo.node_to_component[current_node];
    int head = *(int*)vector_get(ctx->wpo.heads, component_id);

    IntervalState* test_in = interval_new_top_state(ctx->num_locals);
    IntervalState* test_out = interval_new_top_state(ctx->num_locals);

    interval_state_copy(test_in, X_in[head]);
    interval_state_copy(test_out, X_out[head]);

    LOG_DEBUG("HEAD: %d", head);
    BasicBlock* block = *(BasicBlock**)vector_get(ctx->cfg->blocks, head);

    for (int ip = block->ip_start; ip <= block->ip_end; ip++) {
        IrInstruction* ir = *(IrInstruction**)
                                vector_get(ctx->ir_function->ir_instructions, ip);
        interval_transfer(test_out, ir);
    }

    int changed = 0;
    interval_join(test_in, test_out, &changed);

    return !changed;
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
    IntervalState** X_in = calloc(nodes_num, sizeof(IntervalState));
    IntervalState** X_out = calloc(nodes_num, sizeof(IntervalState));

    if (!N || !X_in || !X_out) {
        goto cleanup;
    }

#ifdef DEBUG
    for (int i = 0; i < nodes_num; i++) {
        LOG_DEBUG("NODE %d, COMPONENT %d", i, ctx->wpo.node_to_component[i]);
    }
#endif

    /*** INIT ***/
    int current_node = 0;
    X_in[current_node] = interval_new_top_state(ctx->num_locals);
    X_out[current_node] = interval_new_bottom_state(ctx->num_locals);

    for (int i = current_node + 1; i < nodes_num; i++) {
        X_in[i] = interval_new_bottom_state(ctx->num_locals);
        X_out[i] = interval_new_bottom_state(ctx->num_locals);
    }

    Vector* worklist = vector_new(sizeof(int));
    vector_push(worklist, &current_node);

    LOG_DEBUG("interpreter_abstract_run-------------");
    while (!vector_pop(worklist, &current_node)) {
        LOG_DEBUG("NODE %d STATE:", current_node);
        interval_state_print(X_in[current_node]);

        if (N[current_node] == ctx->wpo.num_sched_pred[current_node]) {
            /*** NonExit ***/
            if (current_node < ctx->block_count) {
                int changed = 0;
                int is_conditional = apply_f(current_node, ctx, X_in, X_out, &changed);

                N[current_node] = 0;

                if (changed) {
                    // update successors
                    update_scheduling_successors(ctx->wpo.wpo, current_node, N, worklist, ctx->wpo.num_sched_pred, X_in, X_out, is_conditional);
                }
            }
            /*** Exit ***/
            else {
                N[current_node] = 0;
                if (is_component_stabilized(current_node, ctx, X_in, X_out)) {
                    update_scheduling_successors(ctx->wpo.wpo, current_node, N, worklist, ctx->wpo.num_sched_pred, X_in, X_out, 0);
                } else {
                    set_n_for_component(N, current_node, ctx, worklist);
                }
            }
        }

        LOG_DEBUG("NODE %d STATE AFTER:", current_node);
        interval_state_print(X_out[current_node]);
    }

    LOG_INFO("RESULTS:");
    for (int i = 0; i < nodes_num; i++) {
        LOG_INFO("NODE %d", i);
        interval_state_print(X_out[i]);
    }

cleanup:
    LOG_ERROR("TODO interpreter abstract run cleanup");
    return NULL;
}
