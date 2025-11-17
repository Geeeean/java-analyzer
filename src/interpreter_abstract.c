#include "interpreter_abstract.h"
#include "cfg.h"
#include "domain_interval.h"
#include "ir_program.h"
#include "log.h"
#include "scc.h"
#include "vector.h"

struct AbstractContext {
    Cfg* cfg;
    int block_count;
    IrFunction* ir_function;
    IntervalState** states;
    CondensedSCC* cscc;
    int num_locals;
    int* N;
    Vector* worklist;
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
    IrFunction* ir_function = ir_program_get_function_ir(m, cfg);
    int num_locals = ir_program_get_num_locals(m, cfg);

    if (!cfg || !ir_function || num_locals < 0) {
        goto cleanup;
    }

    CondensedSCC* cscc = cscc_build(control_flow_graph);
    if (!cscc) {
        goto cleanup;
    }

    AbstractContext* ctx = malloc(sizeof(AbstractContext));
    if (!ctx) {
        goto cleanup;
    }

    ctx->cscc = cscc;
    ctx->ir_function = ir_function;
    ctx->num_locals = num_locals;
    ctx->block_count = vector_length(control_flow_graph->blocks);
    ctx->cfg = control_flow_graph;

    IntervalState** states = malloc(sizeof(IntervalState*) * cscc->wpo_node_count);
    if (!states) {
        goto cleanup;
    }

    int* N = calloc(cscc->wpo_node_count, sizeof(int));
    if (!N) {
        goto cleanup;
    }

    for (int i = 0; i < cscc->wpo_node_count; i++) {
        states[i] = interval_new_top_state(num_locals);
    }

    ctx->states = states;
    ctx->N = N;

    ctx->worklist = vector_new(sizeof(int));
    int entry = 0;
    vector_push(ctx->worklist, &entry);

    return ctx;

cleanup:
    return NULL;
}

typedef struct {
    int is_conditional; // 0 = blocco normale, 1 = condizionale
    IntervalState* out_single; // usato se is_conditional = 0
    IntervalState* out_true; // usato se is_conditional = 1
    IntervalState* out_false; // usato se is_conditional = 1
} BlockTransferResult;

static BlockTransferResult
apply_block_transfer(AbstractContext* ctx, int node)
{
    BlockTransferResult res;
    res.is_conditional = 0;
    res.out_single = NULL;
    res.out_true = NULL;
    res.out_false = NULL;

    Cfg* cfg = ctx->cfg;
    BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, node);

    // Stato di input del blocco = X[node]
    IntervalState* state = interval_new_top_state(ctx->num_locals);
    interval_state_copy(state, ctx->states[node]);

    // Tutte le istruzioni tranne l’ultima
    for (int ip = block->ip_start; ip < block->ip_end; ip++) {
        IrInstruction* ir = *(IrInstruction**)
                                vector_get(ctx->ir_function->ir_instructions, ip);
        interval_transfer(state, ir);
    }

    // Ultima istruzione
    IrInstruction* last = *(IrInstruction**)vector_get(ctx->ir_function->ir_instructions,
        block->ip_end);

    // Caso NON condizionale
    if (!ir_instruction_is_conditional(last)) {
        interval_transfer(state, last);

        res.is_conditional = 0;
        res.out_single = state;
        return res;
    }

    // Caso condizionale
    res.is_conditional = 1;

    IntervalState* out_true = interval_new_top_state(ctx->num_locals);
    IntervalState* out_false = interval_new_top_state(ctx->num_locals);

    interval_state_copy(out_true, state);
    interval_state_copy(out_false, state);

    interval_transfer_conditional(out_true, out_false, last);

    res.out_true = out_true;
    res.out_false = out_false;

    // lo stato di lavoro non serve più
    // interval_state_free(state);

    return res;
}

void interpreter_abstract_run(AbstractContext* ctx)
{
    CondensedSCC* cscc = ctx->cscc;
    int block_count = ctx->block_count;
    int wpo_n = cscc->wpo_node_count;

    int* visited = calloc(wpo_n, sizeof(int));   // copy-first semantics

    while (vector_length(ctx->worklist) > 0) {

        int node;
        vector_pop(ctx->worklist, &node);
        printf("NODE %d\n", node);

        int is_exit = (node >= block_count);

        /* =======================================================
           CASO 1: NODO NORMALE (NON EXIT)
           ======================================================= */
        if (!is_exit) {

            int needed = vector_length(cscc->scheduling_pred[node]);
            if (ctx->N[node] != needed)
                continue;

            ctx->N[node] = 0;

            int comp = cscc->component_of_node[node];
            int head = cscc->head[comp];

            /* ---- Transfer del blocco ---- */
            BlockTransferResult tr = apply_block_transfer(ctx, node);

            /* ---- Caso NON condizionale ---- */
            if (!tr.is_conditional) {

                IntervalState* out = tr.out_single;

                for (int i = 0; i < vector_length(cscc->successors[node]); i++) {
                    int succ = *(int*)vector_get(cscc->successors[node], i);

                    int succ_comp = cscc->component_of_node[succ];
                    int succ_head = cscc->head[succ_comp];

                    int changed = 0;

                    /* widening se succ è head di un ciclo */
                    if (succ == succ_head && cscc->is_loop[succ_comp]) {
                        interval_widening(ctx->states[succ], out, &changed);
                    } else {
                        if (!visited[succ]) {
                            interval_state_copy(ctx->states[succ], out);
                            visited[succ] = 1;
                            changed = 1;
                        } else {
                            interval_join(ctx->states[succ], out, &changed);
                        }
                    }

                    ctx->N[succ]++;
                    int need = vector_length(cscc->scheduling_pred[succ]);
                    if (ctx->N[succ] == need)
                        vector_push(ctx->worklist, &succ);
                }
            }

            /* ---- Caso CONDIZIONALE (due rami) ---- */
            else {

                if (vector_length(cscc->successors[node]) != 2) {
                    printf("ERRORE: nodo %d condizionale ma non ha 2 successori!\n", node);
                    continue;
                }

                int succ_true  = *(int*)vector_get(cscc->successors[node], 0);
                int succ_false = *(int*)vector_get(cscc->successors[node], 1);

                /* ---- TRUE branch ---- */
                {
                    int succ = succ_true;
                    IntervalState* out = tr.out_true;
                    int succ_comp = cscc->component_of_node[succ];
                    int succ_head = cscc->head[succ_comp];

                    int changed = 0;
                    if (succ == succ_head && cscc->is_loop[succ_comp]) {
                        interval_widening(ctx->states[succ], out, &changed);
                    } else {
                        if (!visited[succ]) {
                            interval_state_copy(ctx->states[succ], out);
                            visited[succ] = 1;
                            changed = 1;
                        } else {
                            interval_join(ctx->states[succ], out, &changed);
                        }
                    }

                    ctx->N[succ]++;
                    int need = vector_length(cscc->scheduling_pred[succ]);
                    if (ctx->N[succ] == need)
                        vector_push(ctx->worklist, &succ);
                }

                /* ---- FALSE branch ---- */
                {
                    int succ = succ_false;
                    IntervalState* out = tr.out_false;
                    int succ_comp = cscc->component_of_node[succ];
                    int succ_head = cscc->head[succ_comp];

                    int changed = 0;
                    if (succ == succ_head && cscc->is_loop[succ_comp]) {
                        interval_widening(ctx->states[succ], out, &changed);
                    } else {
                        if (!visited[succ]) {
                            interval_state_copy(ctx->states[succ], out);
                            visited[succ] = 1;
                            changed = 1;
                        } else {
                            interval_join(ctx->states[succ], out, &changed);
                        }
                    }

                    ctx->N[succ]++;
                    int need = vector_length(cscc->scheduling_pred[succ]);
                    if (ctx->N[succ] == need)
                        vector_push(ctx->worklist, &succ);
                }
            }

            continue;
        }

        /* =======================================================
           CASO 2: EXIT NODE
           ======================================================= */

        int exit_index = node - block_count;
        int comp = cscc->exit_id_inverse[exit_index];
        int head = cscc->head[comp];

        int needed = vector_length(cscc->scheduling_pred[node]);
        if (ctx->N[node] != needed)
            continue;

        ctx->N[node] = 0;

        /* ---- Transfer del head SENZA widening ---- */
        BlockTransferResult tr_h = apply_block_transfer(ctx, head);

        IntervalState* new_h = interval_new_top_state(ctx->num_locals);
        int dummy = 0;

        if (!tr_h.is_conditional) {
            interval_state_copy(new_h, tr_h.out_single);
        } else {
            interval_state_copy(new_h, tr_h.out_true);
            interval_join(new_h, tr_h.out_false, &dummy);
        }

        /* ---- Test di stabilizzazione ---- */
        int changed = 0;
        if (!visited[head]) {
            interval_state_copy(ctx->states[head], new_h);
            visited[head] = 1;
            changed = 1;   // prima visita → cambia sempre
        } else {
            interval_join(ctx->states[head], new_h, &changed);
        }

        int stabilized = !changed;

        /* ---- COMPONENTE STABILIZZATA ---- */
        if (stabilized) {

            for (int i = 0; i < vector_length(cscc->successors[node]); i++) {
                int succ = *(int*)vector_get(cscc->successors[node], i);

                ctx->N[succ]++;
                int need = vector_length(cscc->scheduling_pred[succ]);
                if (ctx->N[succ] == need)
                    vector_push(ctx->worklist, &succ);
            }

            continue;
        }

        /* ---- COMPONENTE NON STABILIZZATA ---- */
        for (int i = 0; i < vector_length(cscc->Cx[comp]); i++) {
            int v = *(int*)vector_get(cscc->Cx[comp], i);

            ctx->N[v] = cscc->num_outer_sched_preds[v][comp];

            int need = vector_length(cscc->scheduling_pred[v]);
            if (ctx->N[v] == need)
                vector_push(ctx->worklist, &v);
        }
    }

    /* ----- Stampa finale ----- */
    for (int i = 0; i < block_count; i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(ctx->cfg->blocks, i);
        printf("BLOCK %d: [%d-%d]\n", i, block->ip_start, block->ip_end);

        for (int j = 0; j < vector_length(ctx->states[i]->env); j++) {
            Interval* itv = vector_get(ctx->states[i]->env, j);
            printf("  n%d = [%d, %d]\n", j, itv->lower, itv->upper);
        }
        printf("\n");
    }
}

