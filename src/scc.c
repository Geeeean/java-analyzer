#include "scc.h"
#include "cfg.h"
#include "common.h"
#include "vector.h"

struct SCC {
    int comp_count;
    Vector** components; // sizeof(Vector*) * num_block
    int* comp_id;
};

typedef struct Item Item;
struct Item {
    int value;
    Item* next;
};

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

    if (!cscc->comp_succ
        || !cscc->scc) {
        goto cleanup;
    }

    cscc->scc = scc_build(cfg);
    SCC* scc = cscc->scc;
    cscc->comp_succ = malloc(sizeof(Vector*) * scc->comp_count);

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

cleanup:
    // todo
    return cscc;
}
