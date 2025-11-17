#include "scc.h"
#include "cfg.h"
#include "common.h"
#include "log.h"
#include "vector.h"
#include <limits.h>

typedef struct Item Item;
struct Item {
    int value;
    Item* next;
};

static int min(Vector* v)
{
    int res = INT_MAX;
    for (int i = 0; i < vector_length(v); i++) {
        int* value = vector_get(v, i);
        if (*value < res) {
            res = *value;
        }
    }

    return res;
}

void print_wpo_graph(
    Vector** successors,
    int* stabilizing_pred,
    int wpo_node_count)
{
    printf("=== GRAFO WPO ===\n");

    for (int i = 0; i < wpo_node_count; i++) {
        printf("Nodo %d:\n", i);

        // successori normali e stabilizing
        for (int j = 0; j < vector_length(successors[i]); j++) {
            int v = *(int*)vector_get(successors[i], j);

            // controlla se è un arco di stabilizzazione
            int is_stabilizing = (stabilizing_pred[v] == i);

            if (is_stabilizing) {
                printf("  -> %d   (stabilizing edge)\n", v);
            } else {
                printf("  -> %d\n", v);
            }
        }

        printf("\n");
    }
}

static int stack_push(Item** top, int value)
{
    Item* new_item = malloc(sizeof(Item));
    if (!new_item)
        return FAILURE;

    new_item->value = value;
    new_item->next = *top;
    *top = new_item;
    return SUCCESS;
}

static int stack_pop(Item** top, int* value)
{
    if (!top || !*top)
        return FAILURE;

    Item* temp = *top;
    *value = temp->value;
    *top = temp->next;
    free(temp);
    return SUCCESS;
}

static int stack_is_empty(Item* top)
{
    return top == NULL;
}

static void strong_connect(BasicBlock* block, int index[],
    int low_link[],
    int on_stack[],
    int comp_id[],
    int* current_index,
    int* comp_count,
    Item** stack,
    Vector** components)
{
    int id = block->id;

    index[id] = *current_index;
    low_link[id] = *current_index;

    (*current_index)++;

    stack_push(stack, id);

    on_stack[id] = 1;

    for (int i = 0; i < vector_length(block->successors); i++) {
        BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, i);
        int successor_id = successor->id;

        // not visited
        if (index[successor_id] == -1) {
            strong_connect(successor,
                index,
                low_link,
                on_stack,
                comp_id,
                current_index,
                comp_count,
                stack,
                components);
            low_link[id] = MIN(low_link[id], low_link[successor_id]);
        } else if (on_stack[successor_id]) {
            low_link[id] = MIN(low_link[id], index[successor_id]);
        }
    }

    if (low_link[id] == index[id]) {
        int w;

        do {
            stack_pop(stack, &w);
            on_stack[w] = 0;

            comp_id[w] = *comp_count;
            vector_push(components[*comp_count], &w);

        } while (w != id);
        (*comp_count)++;
    }
}

static SCC* scc_build(Cfg* cfg)
{
    if (!cfg) {
        return NULL;
    }

    int* index = NULL;
    int* low_link = NULL;
    int* on_stack = NULL;
    int* comp_id = NULL;
    Vector** components = NULL;

    int num_block = vector_length(cfg->blocks);

    index = malloc(sizeof(int) * num_block);
    if (!index) {
        goto cleanup;
    }

    low_link = malloc(sizeof(int) * num_block);
    if (!low_link) {
        goto cleanup;
    }

    on_stack = malloc(sizeof(int) * num_block);
    if (!on_stack) {
        goto cleanup;
    }

    comp_id = malloc(sizeof(int) * num_block);
    if (!comp_id) {
        goto cleanup;
    }

    Item* stack = NULL;

    components = malloc(sizeof(Vector*) * num_block);
    if (!components) {
        goto cleanup;
    }

    int comp_count = 0;
    int current_index = 0;

    // init
    for (int i = 0; i < num_block; i++) {
        components[i] = vector_new(sizeof(int));
        if (!components[i]) {
            goto cleanup;
        }

        index[i] = -1; // undefined
        on_stack[i] = 0; // false
        low_link[i] = 0;
    }

    for (int i = 0; i < num_block; i++) {
        if (index[i] == -1) {
            BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
            strong_connect(block,
                index,
                low_link,
                on_stack,
                comp_id,
                &current_index,
                &comp_count,
                &stack,
                components);
        }
    }

cleanup:
    while (!stack_is_empty(stack)) {
        int dummy;
        stack_pop(&stack, &dummy);
    }
    free(index);
    free(low_link);
    free(on_stack);

    if (!components || !comp_id) {
        goto error;
    }

    SCC* scc = malloc(sizeof(SCC));
    if (!scc) {
        goto error;
    }
    scc->comp_count = comp_count;
    scc->comp_id = comp_id;
    scc->components = components;

    return scc;

error:
    if (components) {
        for (int i = 0; i < num_block; i++) {
            vector_delete(components[i]);
        }
        free(components);
    }
    if (comp_id) {
        free(comp_id);
    }

    return NULL;
}

CondensedSCC* cscc_build(Cfg* cfg)
{
    if (!cfg) {
        goto cleanup;
    }

    CondensedSCC* cscc = malloc(sizeof(CondensedSCC));
    if (!cscc) {
        goto cleanup;
    }

    cscc->scc = scc_build(cfg);

    if (!cscc->scc) {
        goto cleanup;
    }
    SCC* scc = cscc->scc;

    cscc->comp_succ = malloc(sizeof(Vector*) * scc->comp_count);
    if (!cscc->comp_succ) {
        goto cleanup;
    }

    for (int i = 0; i < scc->comp_count; i++) {
        cscc->comp_succ[i] = vector_new(sizeof(int));
    }

    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        int c_from = scc->comp_id[block->id];

        for (int j = 0; j < vector_length(block->successors); j++) {
            BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, j);
            int c_to = scc->comp_id[successor->id];

            if (c_from != c_to) {
                // todo: avoid duplicates
                vector_push(cscc->comp_succ[c_from], &c_to);
            }
        }
    }

    /*** HEAD + IS LOOP***/
    int* head = malloc(sizeof(int) * cscc->scc->comp_count);
    if (!head) {
        goto cleanup;
    }

    uint8_t* is_loop = calloc(cscc->scc->comp_count, sizeof(uint8_t));

    for (int i = 0; i < cscc->scc->comp_count; i++) {
        head[i] = min(cscc->scc->components[i]);

        if (vector_length(cscc->scc->components[i]) > 1) {
            is_loop[i] = 1;
        } else if (vector_length(cscc->scc->components[i]) == 1) {
            int b = *(int*)vector_get(cscc->scc->components[i], 0);
            BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, b);

            int self_loop = 0;
            for (int j = 0; j < vector_length(block->successors); j++) {
                BasicBlock* succ = *(BasicBlock**)vector_get(block->successors, j);
                if (succ->id == block->id) {
                    self_loop = 1;
                    break;
                }
            }

            is_loop[i] = self_loop;
        }
    }

    cscc->head = head;
    cscc->is_loop = is_loop;

    /*** EXIT ***/
    int* exit_id = malloc(sizeof(int) * scc->comp_count);
    int* exit_id_inverse = malloc(sizeof(int) * scc->comp_count);
    if (!exit_id || !exit_id_inverse) {
        goto cleanup;
    }

    int wpo_node_count = vector_length(cfg->blocks);

    for (int i = 0; i < scc->comp_count; i++) {
        if (!is_loop[i]) {
            exit_id[i] = -1;
        } else {
            exit_id[i] = wpo_node_count;
            exit_id_inverse[wpo_node_count - vector_length(cfg->blocks)] = i;
            wpo_node_count++;
        }
    }

    Vector** successors = malloc(sizeof(Vector*) * wpo_node_count);
    if (!successors) {
        goto cleanup;
    }

    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        successors[i] = vector_new(sizeof(int));
        if (!successors[i]) {
            goto cleanup;
        }

        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        for (int j = 0; j < vector_length(block->successors); j++) {
            BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, j);

            vector_push(successors[i], &successor->id);
        }
    }

    for (int i = vector_length(cfg->blocks); i < wpo_node_count; i++) {
        successors[i] = vector_new(sizeof(int));
        if (!successors[i]) {
            goto cleanup;
        }

        int exit_index = i - vector_length(cfg->blocks);
        int c = exit_id_inverse[exit_index];

        vector_push(successors[i], &cscc->head[c]);

        Vector* comp = cscc->scc->components[c];

        vector_push(successors[head[c]], &i);

        for (int k = 0; k < vector_length(comp); k++) {
            int b = *(int*)vector_get(comp, k);
            BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, b);

            for (int j = 0; j < vector_length(block->successors); j++) {
                BasicBlock* succ = *(BasicBlock**)vector_get(block->successors, j);
                int c_to = cscc->scc->comp_id[succ->id];

                if (c_to != c) {
                    int head_to = cscc->head[c_to];
                    vector_push(successors[i], &head_to);
                }
            }
        }
    }

    cscc->wpo_node_count = wpo_node_count;
    cscc->successors = successors;

    int* stabilizing_pred = malloc(sizeof(int) * wpo_node_count);
    if (!stabilizing_pred) {
        goto cleanup;
    }

    for (int i = 0; i < wpo_node_count; i++) {
        stabilizing_pred[i] = -1;
    }

    for (int c = 0; c < scc->comp_count; c++) {
        if (!is_loop[c]) {
            continue;
        }

        int x = exit_id[c];
        int h = head[c];

        if (x >= 0) {
            stabilizing_pred[h] = x;
        }
    }

    /*** COMPONENT OF EACH WPO NODE (Cx) ***/
    int* component_of_node = malloc(sizeof(int) * wpo_node_count);
    if (!component_of_node) {
        goto cleanup;
    }

    int block_count = vector_length(cfg->blocks);

    /* 1) Per i nodi normali del CFG (0 .. block_count-1) */
    for (int v = 0; v < block_count; v++) {
        component_of_node[v] = scc->comp_id[v];
    }

    /* 2) Per gli exit nodes (block_count .. wpo_node_count-1) */
    for (int x = block_count; x < wpo_node_count; x++) {
        int exit_index = x - block_count; // indice nell’array degli exit
        int c = exit_id_inverse[exit_index]; // componente a cui appartiene questo exit
        component_of_node[x] = c;
    }

    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);

        for (int j = 0; j < vector_length(block->successors); j++) {
            BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, j);
            int component = component_of_node[successor->id];
            if (is_loop[component] && component_of_node[successor->id] != component_of_node[i]) {
                vector_push(successors[i], &exit_id[component]);
            }
        }
    }

    for (int i = 0; i < scc->comp_count; i++) {
        int component = i;
        // if im in a loop
        if (is_loop[component]) {
            // iterate over each node in the loop
            for (int j = 0; j < vector_length(cscc->scc->components[component]); j++) {
                int go_to_head = 0;
                int node = *(int*)vector_get(cscc->scc->components[component], j);

                int loop_head = head[component];

                for (int k = 0; k < vector_length(successors[node]); k++) {
                    int node_to = *(int*)vector_get(successors[node], k);
                    if (node_to == loop_head) {
                        go_to_head = 1;
                        break;
                    }
                }

                if (go_to_head) {
                    vector_push(successors[node], &exit_id[component]);
                }
            }
        }
    }

    /*** SCHEDULING PREDECESSORS ***/
    Vector** scheduling_pred = malloc(sizeof(Vector*) * wpo_node_count);
    if (!scheduling_pred) {
        goto cleanup;
    }

    for (int i = 0; i < wpo_node_count; i++) {
        scheduling_pred[i] = vector_new(sizeof(int));
        if (!scheduling_pred[i]) {
            goto cleanup;
        }
    }

    for (int i = 0; i < wpo_node_count; i++) {
        for (int j = 0; j < vector_length(successors[i]); j++) {
            int* v = vector_get(successors[i], j);

            // avoid exit -> head
            if (stabilizing_pred[*v] == i) {
                LOG_DEBUG("%d %d", *v, i);
                continue;
            }

            if (component_of_node[*v] == component_of_node[i] && head[component_of_node[i]] == *v) {
                continue;
            }

            vector_push(scheduling_pred[*v], &i);
        }
    }

    /*** NUM OUTER SCHED PREDS ***/
    int** num_outer_sched_preds = malloc(sizeof(int*) * wpo_node_count);
    if (!num_outer_sched_preds) {
        goto cleanup;
    }

    for (int v = 0; v < wpo_node_count; v++) {
        num_outer_sched_preds[v] = calloc(scc->comp_count, sizeof(int));
        if (!num_outer_sched_preds[v]) {
            goto cleanup;
        }
    }

    for (int v = 0; v < wpo_node_count; v++) {
        for (int i = 0; i < vector_length(scheduling_pred[v]); i++) {
            int u = *(int*)vector_get(scheduling_pred[v], i);

            int cu = component_of_node[u]; // component of predecessor
            int cv = component_of_node[v]; // component of v

            for (int c = 0; c < scc->comp_count; c++) {
                if (cu != c) {
                    num_outer_sched_preds[v][c]++;
                }
            }
        }
    }

    /*** Cx: NODES FOR EACH WPO COMPONENT ***/
    Vector** Cx = malloc(sizeof(Vector*) * scc->comp_count);
    if (!Cx) {
        goto cleanup;
    }

    for (int c = 0; c < scc->comp_count; c++) {
        Cx[c] = vector_new(sizeof(int));
        if (!Cx[c]) {
            goto cleanup;
        }
    }

    for (int v = 0; v < block_count; v++) {
        int c = scc->comp_id[v];
        vector_push(Cx[c], &v);
    }

    for (int c = 0; c < scc->comp_count; c++) {
        int x = exit_id[c];
        if (x >= 0) { // esiste un exit → componente ciclica
            vector_push(Cx[c], &x);
        }
    }

    cscc->scheduling_pred = scheduling_pred;
    cscc->component_of_node = component_of_node;

    cscc->exit_id_inverse = exit_id_inverse;
    cscc->Cx = Cx;
    cscc->num_outer_sched_preds = num_outer_sched_preds;

#ifdef DEBUG
    print_wpo_graph(successors, stabilizing_pred, wpo_node_count);
#endif

cleanup:
    // todo
    return cscc;
}
