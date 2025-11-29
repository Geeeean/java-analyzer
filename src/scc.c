#include "scc.h"
#include "common.h"
#include "log.h"
#include "vector.h"
#include <limits.h>

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

static void strong_connect(int id,
    int index[],
    int low_link[],
    int on_stack[],
    int comp_id[],
    int* current_index,
    int* comp_count,
    Item** stack,
    Vector** components,
    Graph* graph)
{
    if (graph->not_valid[id]) {
        comp_id[id] = -1;
        return;
    }

    Node* node = vector_get(graph->nodes, id);

    index[id] = *current_index;
    low_link[id] = *current_index;

    (*current_index)++;

    stack_push(stack, id);

    on_stack[id] = 1;

    for (int i = 0; i < vector_length(node->successors); i++) {
        int successor_id = *(int*)vector_get(node->successors, i);
        if (!graph || !graph->not_valid) {
            LOG_ERROR("Graph or not_valid array is NULL in strong_connect.");
            return;
        }

        if (!index) {
            LOG_ERROR("index array is NULL in strong_connect.");
            return;
        }

        // not visited
        if (index[successor_id] == -1) {
            strong_connect(successor_id,
                index,
                low_link,
                on_stack,
                comp_id,
                current_index,
                comp_count,
                stack,
                components,
                graph);
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

SCC* scc_build(Graph* graph)
{
    if (!graph) {
        return NULL;
    }

    int* index = NULL;
    int* low_link = NULL;
    int* on_stack = NULL;
    int* comp_id = NULL;
    Vector** components = NULL;

    int num_nodes = vector_length(graph->nodes);

    index = malloc(sizeof(int) * num_nodes);
    if (!index) {
        goto cleanup;
    }

    low_link = malloc(sizeof(int) * num_nodes);
    if (!low_link) {
        goto cleanup;
    }

    on_stack = malloc(sizeof(int) * num_nodes);
    if (!on_stack) {
        goto cleanup;
    }

    comp_id = malloc(sizeof(int) * num_nodes);
    if (!comp_id) {
        goto cleanup;
    }

    Item* stack = NULL;

    components = malloc(sizeof(Vector*) * num_nodes);
    if (!components) {
        goto cleanup;
    }

    int comp_count = 0;
    int current_index = 0;

    // init
    for (int i = 0; i < num_nodes; i++) {
        components[i] = vector_new(sizeof(int));
        if (!components[i]) {
            goto cleanup;
        }

        index[i] = -1; // undefined
        on_stack[i] = 0; // false
        low_link[i] = 0;
    }

    for (int i = 0; i < num_nodes; i++) {
        if (index[i] == -1) {
            strong_connect(i,
                index,
                low_link,
                on_stack,
                comp_id,
                &current_index,
                &comp_count,
                &stack,
                components,
                graph);
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
    scc->num_nodes = num_nodes;

    return scc;

error:
    if (components) {
        for (int i = 0; i < num_nodes; i++) {
            vector_delete(components[i]);
        }
        free(components);
    }
    if (comp_id) {
        free(comp_id);
    }

    return NULL;
}

void scc_print(SCC* scc)
{
    for (int i = 0; i < scc->comp_count; i++) {
        Vector* component = scc->components[i];
        LOG_INFO("COMPONENT %d", i);
        for (int j = 0; j < vector_length(component); j++) {
            int node = *(int*)vector_get(component, j);
            LOG_INFO("%d", node);
        }
    }
}

void scc_delete(SCC* scc)
{
    if (scc) {
        free(scc->comp_id);
        for (int i = 0; i < scc->num_nodes; i++) {
            vector_delete(scc->components[i]);
        }
        free(scc->components);
        free(scc);
    }
}
