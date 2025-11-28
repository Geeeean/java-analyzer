#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "graph.h"
#include "ir_program.h"
#include "log.h"
#include "wpo.h"
#include <limits.h>

int x = 0;

struct AbstractContext {
    Cfg* cfg;
    WPO wpo;
    int block_count;
    int exit_count;
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
    LOG_DEBUG("BEFORE");
    cfg_print(control_flow_graph);
#endif

    cfg_inline(control_flow_graph, (Config*)cfg, (Method*)m);

#ifdef DEBUG
    LOG_DEBUG("AFTER");
    cfg_print(control_flow_graph);
#endif

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

void apply_last(IrInstruction* last,
    BasicBlock* block,
    IntervalState** X_in,
    IntervalState** X_out,
    IntervalState* out,
    int current_node,
    int component,
    AbstractContext* ctx,
    int head)
{
    int dummy = 0;

    if (ir_instruction_is_conditional(last)) {
        IntervalState* out_true = interval_new_top_state(block->num_locals);
        IntervalState* out_false = interval_new_top_state(block->num_locals);

        interval_state_copy(out_true, out);
        interval_state_copy(out_false, out);

        interval_transfer_conditional(out_true, out_false, last);

        Node* node = vector_get(ctx->wpo.wpo->nodes, current_node);

        int* successor_true;
        int* successor_false;

        if (head) {
            int* exit_id = vector_get(ctx->wpo.exits, component);
            Node* exit_node = vector_get(ctx->wpo.wpo->nodes, *exit_id);

            successor_true = vector_get(exit_node->successors, 0);
            successor_false = vector_get(node->successors, 0);
        } else {
            successor_true = vector_get(node->successors, 0);
            successor_false = vector_get(node->successors, 1);
        }

        if (successor_true) {
            interval_join(X_in[*successor_true], out_true, &dummy);
        }

        if (successor_false) {
            interval_join(X_in[*successor_false], out_false, &dummy);
        }
    } else if (last->opcode == OP_INVOKE) {
        IntervalState* state = interval_new_top_state(0);
        Node* node = vector_get(ctx->wpo.wpo->nodes, current_node);
        int invoke_head = *(int*)vector_get(node->successors, 0);

        BasicBlock* successor = *(BasicBlock**)vector_get(ctx->cfg->blocks, invoke_head);

        interval_transfer_invoke(state, X_out[current_node], successor->num_locals);

        interval_join(X_in[invoke_head], state, &dummy);
    } else if (last->opcode == OP_RETURN) {
        if (vector_length(X_out[current_node]->stack)) {
            int iv_id;
            vector_pop(X_out[current_node]->stack, &iv_id);
            Interval* iv = vector_get(X_out[current_node]->env, iv_id);
        }
    } else {
        interval_transfer(out, last);
    }
}

int apply_f(int current_node, AbstractContext* ctx, IntervalState** X_in, IntervalState** X_out, int* visited)
{
    int dummy;
    int component = ctx->wpo.node_to_component[current_node];
    int head = component == -1 ? -1 : *(int*)vector_get(ctx->wpo.heads, component);

    IntervalState* in = X_in[current_node];
    IntervalState* out = X_out[current_node];

    BasicBlock* block = *(BasicBlock**)vector_get(ctx->cfg->blocks, current_node);
    IrInstruction* last = *(IrInstruction**)vector_get(block->ir_function->ir_instructions,
        block->ip_end);

    if (head == current_node) {
        if (is_interval_state_bottom(out)) {
            interval_state_copy(out, in);
        } else {
            interval_widening(out, in, &dummy);
        }

        for (int ip = block->ip_start; ip < block->ip_end; ip++) {
            IrInstruction* ir = *(IrInstruction**)
                                    vector_get(block->ir_function->ir_instructions, ip);
            interval_transfer(out, ir);
        }

        apply_last(last, block, X_in, X_out, out, current_node, component, ctx, 1);
    } else {
        interval_state_copy(out, in);

        for (int ip = block->ip_start; ip < block->ip_end; ip++) {
            IrInstruction* ir = *(IrInstruction**)
                                    vector_get(block->ir_function->ir_instructions, ip);
            interval_transfer(out, ir);
        }
        apply_last(last, block, X_in, X_out, out, current_node, component, ctx, 0);

        interval_join(in, out, &dummy);
    }

    return ir_instruction_is_conditional(last) || last->opcode == OP_INVOKE;
}

int is_component_stabilized(int current_node, AbstractContext* ctx, IntervalState** X_in, IntervalState** X_out)
{
    int component_id = ctx->wpo.node_to_component[current_node];
    int head = *(int*)vector_get(ctx->wpo.heads, component_id);

    BasicBlock* block = vector_get(ctx->cfg->blocks, head);

    IntervalState* test_in = interval_new_top_state(block->num_locals);
    IntervalState* test_out = interval_new_top_state(block->num_locals);

    interval_state_copy(test_in, X_in[head]);
    interval_state_copy(test_out, X_out[head]);

    if (vector_length(test_in->locals) != vector_length(test_out->locals)) {
        return 0;
    }

    for (int i = 0; i < vector_length(test_in->locals); i++) {
        int in_id = *(int*)vector_get(test_in->locals, i);
        int out_id = *(int*)vector_get(test_out->locals, i);

        Interval* in = vector_get(test_in->env, in_id);
        Interval* out = vector_get(test_out->env, out_id);

        LOG_DEBUG("IN: [%d, %d]", in->lower, in->upper);
        LOG_DEBUG("out: [%d, %d]", out->lower, out->upper);

        if (out->lower == in->lower && out->upper == in->upper && (in->lower == INT_MIN || in->upper == INT_MAX)) {
            continue;
        }

        if ((out->lower < in->lower) || (out->upper > in->upper)) {
            return 0;
        }
    }

    return 1;
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
    int* visited = calloc(nodes_num, sizeof(int));
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
    BasicBlock* entry_block = *(BasicBlock**)vector_get(ctx->cfg->blocks, 0);

    X_in[current_node] = interval_new_top_state(entry_block->num_locals);
    X_out[current_node] = interval_new_bottom_state(entry_block->num_locals);

    for (int i = current_node + 1; i < nodes_num; i++) {
        BasicBlock* block;
        LOG_DEBUG("NODE: %d", i);

        // node is an exit
        if (i < ctx->block_count) {
            block = *(BasicBlock**)vector_get(ctx->cfg->blocks, i);
        } else {
            int exit_component = ctx->wpo.node_to_component[i];
            int head = *(int*)vector_get(ctx->wpo.heads, exit_component);
            block = *(BasicBlock**)vector_get(ctx->cfg->blocks, head);
        }

        X_in[i] = interval_new_bottom_state(block->num_locals);
        X_out[i] = interval_new_bottom_state(block->num_locals);
    }

    Vector* worklist = vector_new(sizeof(int));
    vector_push(worklist, &current_node);

    while (!vector_pop(worklist, &current_node)) {
#ifdef DEBUG
        LOG_DEBUG("NODE %d STATE BEFORE:", current_node);
        interval_state_print(X_in[current_node]);
#endif
        if (N[current_node] == ctx->wpo.num_sched_pred[current_node]) {
            /*** NonExit ***/
            if (current_node < ctx->block_count) {
                int is_conditional = apply_f(current_node, ctx, X_in, X_out, visited);

                N[current_node] = 0;

                update_scheduling_successors(ctx->wpo.wpo, current_node, N, worklist, ctx->wpo.num_sched_pred, X_in, X_out, is_conditional);
            }
            /*** Exit ***/
            else {
                interval_state_copy(X_out[current_node], X_in[current_node]);
                N[current_node] = 0;
                if (is_component_stabilized(current_node, ctx, X_in, X_out)) {
                    Node* node = vector_get(ctx->wpo.wpo->nodes, current_node);
                    // update_scheduling_successors(ctx->wpo.wpo, current_node, N, worklist, ctx->wpo.num_sched_pred, X_in, X_out, 0);
                    int exit_component = ctx->wpo.node_to_component[current_node];
                    int head = *(int*)vector_get(ctx->wpo.heads, exit_component);
                    for (int i = 0; i < vector_length(node->successors); i++) {
                        int successor = *(int*)vector_get(node->successors, i);
                        if (successor != head) {
                            N[successor]++;
                            if (N[successor] == ctx->wpo.num_sched_pred[successor]) {
                                vector_push(worklist, &successor);
                                int dummy;
                                interval_join(X_in[successor], X_out[current_node], &dummy);
                            }
                        }
                    }
                } else {
                    // update cycle successor
                    Node* node = vector_get(ctx->wpo.wpo->nodes, current_node);
                    int node_id = *(int*)vector_get(node->successors, vector_length(node->successors) - 1);
                    int dummy = 0;
                    interval_join(X_in[node_id], X_out[current_node], &dummy);
                    set_n_for_component(N, current_node, ctx, worklist);
                }
            }

#ifdef DEBUG
            LOG_DEBUG("worklist:\n");
            for (int i = 0; i < vector_length(worklist); i++) {
                LOG_DEBUG("%d ", *(int*)vector_get(worklist, i));
            }
#endif
        }

#ifdef DEBUG
        LOG_DEBUG("NODE %d STATE AFTER:", current_node);
        interval_state_print(X_out[current_node]);
#endif
        visited[current_node] = 1;
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
