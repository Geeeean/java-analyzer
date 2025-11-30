#include "wpo.h"
#include "common.h"
#include "pair.h"
#include "scc.h"
#include "stdlib.h"
#include "vector.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

struct WPOComponent {
    Vector* nodes; // Vector<int>
    Vector* exits; // Vector<int>
    Vector* scheduling_edges; //  Vector<Pair>
    Vector* stabilizing_edges; // Vector<Pair>
    int head;
    int exit;
};

int get_new_exit(int* exit_id)
{
    int new_exit = *exit_id;
    (*exit_id)++;

    return new_exit;
}

static Vector** new_edges_vector(int len)
{
    if (len == 0) {
        return NULL;
    }

    Vector** edges = malloc(sizeof(Vector*) * len);
    for (int i = 0; i < len; i++) {
        edges[i] = vector_new(sizeof(int));
    }

    return edges;
}

static WPOComponent trivial_WPO(int node)
{
    int head = node;
    Vector* nodes = vector_new(sizeof(int));
    vector_push(nodes, &node);

    return (WPOComponent) {
        .nodes = nodes,
        .exits = vector_new(sizeof(int)),
        .scheduling_edges = vector_new(sizeof(Pair)),
        .stabilizing_edges = vector_new(sizeof(Pair)),
        .head = head,
        .exit = head,
    };
}

static WPOComponent self_loop_WPO(int node, int* exit_id)
{
    int head = node;
    int new_exit_id = *exit_id;
    (*exit_id)++;

    Vector* nodes = vector_new(sizeof(int));
    vector_push(nodes, &node);

    Vector* exits = vector_new(sizeof(int));
    vector_push(exits, &new_exit_id);

    Vector* scheduling_edges = vector_new(sizeof(Pair));
    Pair edge = { .first = head, .second = new_exit_id };
    vector_push(scheduling_edges, &edge);

    Vector* stabilizing_edges = vector_new(sizeof(Pair));
    Pair edge2 = { .first = new_exit_id, .second = head };
    vector_push(stabilizing_edges, &edge2);

    return (WPOComponent) {
        .nodes = nodes,
        .exits = exits,
        .scheduling_edges = scheduling_edges,
        .stabilizing_edges = stabilizing_edges,
        .head = head,
        .exit = new_exit_id,
    };
}

static int
compute_b_len(GraphMathRepr* graph, int head)
{
    int res = 0;
    for (int i = 0; i < vector_length(graph->edges); i++) {
        Pair* edge = vector_get(graph->edges, i);
        if (edge->second == head) {
            res++;
        }
    }

    return res;
}

WPOComponent sccWPO(GraphMathRepr* graph, int* exit_index, Vector* Cx, Vector* heads, Vector* exits)
{
    int head = min(graph->nodes);

    if (compute_b_len(graph, head) == 0) {
        WPOComponent result = trivial_WPO(*(int*)vector_get(graph->nodes, 0));
        return result;
    } else if (vector_length(graph->nodes) == 1) {
        C c = { .components = vector_new(sizeof(int)) };
        vector_push(Cx, &c);

        // pushing a new component
        C* c_last = vector_get(Cx, vector_length(Cx) - 1);
        vector_push(c_last->components, &head);
        vector_push(c_last->components, exit_index);

        // pushing component's head
        vector_push(heads, &head);
        vector_push(exits, exit_index);

        WPOComponent result = self_loop_WPO(*(int*)vector_get(graph->nodes, 0), exit_index);

        return result;
    }

    int new_exit = *exit_index;
    C c = { .components = vector_new(sizeof(int)) };

    vector_push(Cx, &c);
    C* c_last = vector_get(Cx, vector_length(Cx) - 1);

    // pushing component's head
    vector_push(heads, &head);
    vector_push(exits, exit_index);

    (*exit_index)++;

    GraphMathRepr* graph_mr_comp = malloc(sizeof(GraphMathRepr));
    graph_mr_comp->edges = vector_new(sizeof(Pair));
    graph_mr_comp->nodes = vector_new(sizeof(int));

    for (int i = 0; i < vector_length(graph->nodes); i++) {
        int node = *(int*)vector_get(graph->nodes, i);
        if (node != head) {
            vector_push(graph_mr_comp->nodes, &node);
        }
    }

    vector_push(graph_mr_comp->nodes, &new_exit);

    for (int i = 0; i < vector_length(graph->edges); i++) {
        Pair* edge = vector_get(graph->edges, i);
        if (edge->second == head) {
            Pair new_edge = { .first = edge->first, .second = new_exit };
            vector_push(graph_mr_comp->edges, &new_edge);
        } else if (edge->first != head) {
            vector_push(graph_mr_comp->edges, edge);
        }
    }

    WPOComponent wpo_component = wpo_construct(graph_mr_comp, exit_index, Cx, heads, exits);

    WPOComponent result = {
        .nodes = vector_new(sizeof(int)),
        .exits = vector_new(sizeof(int)),
        .exit = new_exit,
        .head = head,
        .scheduling_edges = vector_new(sizeof(Pair)),
        .stabilizing_edges = vector_new(sizeof(Pair)),
    };

    // vector_copy(result.nodes, wpo_component.nodes);
    vector_copy(c_last->components, wpo_component.exits);
    for (int i = 0; i < vector_length(wpo_component.nodes); i++) {
        int node = *(int*)vector_get(wpo_component.nodes, i);
        if (node != new_exit) {
            vector_push(result.nodes, &node);
        }

        vector_push(c_last->components, &node);
    }
    vector_push(c_last->components, &head);
    vector_push(result.nodes, &head);

    vector_copy(result.exits, wpo_component.exits);
    vector_push(result.exits, &new_exit);

    vector_copy(result.scheduling_edges, wpo_component.scheduling_edges);
    for (int i = 0; i < vector_length(graph->edges); i++) {
        Pair* edge = vector_get(graph->edges, i);
        if (edge->first == head) {
            vector_push(result.scheduling_edges, edge);
        }
    }

    vector_copy(result.stabilizing_edges, wpo_component.stabilizing_edges);
    Pair exit_edge = { .first = new_exit, .second = head };
    vector_push(result.stabilizing_edges, &exit_edge);

    vector_delete(wpo_component.nodes);
    vector_delete(wpo_component.exits);
    vector_delete(wpo_component.scheduling_edges);
    vector_delete(wpo_component.stabilizing_edges);

    return result;
}

WPOComponent wpo_construct(GraphMathRepr* graph_mr, int* exit_index, Vector* Cx, Vector* heads, Vector* exits_vec)
{
    Graph* graph = graph_from_graph_math_repr(graph_mr);
    SCC* scc = scc_build(graph);

    WPOComponent result = {
        .nodes = vector_new(sizeof(int)),
        .exits = vector_new(sizeof(int)),
        .scheduling_edges = vector_new(sizeof(Pair)),
        .stabilizing_edges = vector_new(sizeof(Pair)),
        .exit = -1,
        .head = -1,
    };

    // int* heads = malloc(scc->comp_count * sizeof(int));
    int* exits = malloc(scc->comp_count * sizeof(int));

    for (int i = 0; i < scc->comp_count; i++) {
        GraphMathRepr* graph_mr_comp = malloc(sizeof(GraphMathRepr));
        graph_mr_comp->edges = vector_new(sizeof(Pair));
        graph_mr_comp->nodes = vector_new(sizeof(int));

        uint8_t* in_component = calloc(vector_length(graph->nodes), sizeof(uint8_t));

        for (int j = 0; j < vector_length(scc->components[i]); j++) {
            int node = *(int*)vector_get(scc->components[i], j);
            in_component[node] = 1;
        }

        for (int j = 0; j < vector_length(scc->components[i]); j++) {
            int node_id = *(int*)vector_get(scc->components[i], j);
            vector_push(graph_mr_comp->nodes, &node_id);
            Node* node = vector_get(graph->nodes, node_id);

            for (int k = 0; k < vector_length(node->successors); k++) {
                int successor = *(int*)vector_get(node->successors, k);
                if (in_component[successor]) {
                    Pair edge = {
                        .first = node_id,
                        .second = successor
                    };

                    vector_push(graph_mr_comp->edges, &edge);
                }
            }
        }

        WPOComponent wpo_component = sccWPO(graph_mr_comp, exit_index, Cx, heads, exits_vec);
        for (int j = 0; j < vector_length(wpo_component.nodes); j++) {
            vector_push(result.nodes, vector_get(wpo_component.nodes, j));
        }

        for (int j = 0; j < vector_length(wpo_component.exits); j++) {
            vector_push(result.exits, vector_get(wpo_component.exits, j));
        }

        for (int j = 0; j < vector_length(wpo_component.scheduling_edges); j++) {
            Pair* edge = vector_get(wpo_component.scheduling_edges, j);
            vector_push(result.scheduling_edges, edge);
        }

        for (int j = 0; j < vector_length(wpo_component.stabilizing_edges); j++) {
            Pair* edge = vector_get(wpo_component.stabilizing_edges, j);
            vector_push(result.stabilizing_edges, edge);
        }

        // heads[i] = wpo_component.head;
        exits[i] = wpo_component.exit;

        free(in_component);

        vector_delete(wpo_component.nodes);
        vector_delete(wpo_component.exits);
        vector_delete(wpo_component.scheduling_edges);
        vector_delete(wpo_component.stabilizing_edges);

        vector_delete(graph_mr_comp->nodes);
        vector_delete(graph_mr_comp->edges);
        free(graph_mr_comp);
    }

    for (int i = 0; i < vector_length(graph_mr->edges); i++) {
        Pair* edge = vector_get(graph_mr->edges, i);
        if (graph->not_valid[edge->first] || graph->not_valid[edge->second]) {
            continue;
        }

        if (scc->comp_id[edge->first] != scc->comp_id[edge->second]) {
            Pair new_edge = { .first = exits[scc->comp_id[edge->first]], .second = edge->second };
            vector_push(result.scheduling_edges, &new_edge);
        }
    }

cleanup:
    free(exits);
    scc_delete(scc);
    graph_delete(graph);

    return result;
}

int wpo_construct_aux(Graph* graph, WPO* wpo)
{
    Vector* Cx = vector_new(sizeof(C));
    if (!Cx) {
        goto cleanup;
    }

    Vector* heads = vector_new(sizeof(int));
    if (!heads) {
        goto cleanup;
    }

    Vector* exits = vector_new(sizeof(int));
    if (!exits) {
        goto cleanup;
    }

    int exit_index = vector_length(graph->nodes);
    GraphMathRepr* graph_mr = graph_math_repr_from_graph(graph);
    if (!graph_mr) {
        goto cleanup;
    }

    WPOComponent result = wpo_construct(graph_mr, &exit_index, Cx, heads, exits);

    Graph* graph_result = malloc(sizeof(Graph));
    if (!graph_result) {
        goto cleanup;
    }

    int total_nodes = vector_length(result.nodes) + vector_length(result.exits);
    graph_result->nodes = vector_new(sizeof(Node));
    graph_result->not_valid = calloc(total_nodes, sizeof(uint8_t));

    for (int i = 0; i < total_nodes; i++) {
        Node* node = malloc(sizeof(Node));
        node->successors = vector_new(sizeof(int));

        vector_push(graph_result->nodes, node);
    }

    for (int i = 0; i < vector_length(result.scheduling_edges); i++) {
        Pair* edge = vector_get(result.scheduling_edges, i);

        Node* node = vector_get(graph_result->nodes, edge->first);
        vector_push(node->successors, &edge->second);
    }

    for (int i = 0; i < vector_length(result.stabilizing_edges); i++) {
        Pair* edge = vector_get(result.stabilizing_edges, i);

        Node* node = vector_get(graph_result->nodes, edge->first);
        vector_push(node->successors, &edge->second);
    }

    int num_nodes = vector_length(graph_result->nodes);
    int* num_sched_pred = calloc(num_nodes, sizeof(int));
    if (!num_sched_pred) {
        goto cleanup;
    }

    for (int i = 0; i < vector_length(result.scheduling_edges); i++) {
        Pair* edge = vector_get(result.scheduling_edges, i);
        num_sched_pred[edge->second]++;
    }

    int* node_to_component = malloc(num_nodes * sizeof(int));

    for (int i = 0; i < num_nodes; i++) {
        node_to_component[i] = -1;
    }

    for (int i = 0; i < vector_length(Cx); i++) {
        C* component = vector_get(Cx, i);
        for (int j = 0; j < vector_length(component->components); j++) {
            int node = *(int*)vector_get(component->components, j);
            node_to_component[node] = i;
        }
    }

    int num_components = vector_length(Cx);
    int** num_outer_sched_pred = malloc(sizeof(int*) * num_components);
    if (!num_outer_sched_pred) {
        goto cleanup;
    }

    for (int i = 0; i < num_components; i++) {
        num_outer_sched_pred[i] = calloc(num_nodes, sizeof(int));
    }

    // todo speed up this
    for (int i = 0; i < num_components; i++) {
        C* component = vector_get(Cx, i);
        for (int j = 0; j < vector_length(result.scheduling_edges); j++) {
            Pair* edge = vector_get(result.scheduling_edges, j);
            int u = edge->first;
            int v = edge->second;

            int is_u_here = 0;
            int is_v_here = 0;
            for (int k = 0; k < vector_length(component->components); k++) {
                int node = *(int*)vector_get(component->components, k);
                if (node == u) {
                    is_u_here = 1;
                } else if (node == v) {
                    is_v_here = 1;
                }
            }

            if (is_u_here && is_v_here) {
                continue;
            }

            if (is_v_here) {
                num_outer_sched_pred[i][v]++;
            }
        }
    }

#ifdef DEBUG
    for (int i = 0; i < num_components; i++) {
        for (int j = 0; j < num_nodes; j++) {
            printf("%2d ", j);
        }
        printf("\n");

        for (int j = 0; j < num_nodes; j++) {
            printf("%2d ", num_outer_sched_pred[i][j]);
        }
        printf("\n");
    }
#endif

    wpo->num_sched_pred = num_sched_pred;
    wpo->num_outer_sched_pred = num_outer_sched_pred;
    wpo->node_to_component = node_to_component;

    wpo->wpo = graph_result;
    wpo->Cx = Cx;
    wpo->heads = heads;
    wpo->exits = exits;

cleanup:
    vector_delete(graph_mr->nodes);
    vector_delete(graph_mr->edges);
    free(graph_mr);

    vector_delete(result.nodes);
    vector_delete(result.exits);
    vector_delete(result.scheduling_edges);
    vector_delete(result.stabilizing_edges);

    return SUCCESS;
}

void wpo_delete(WPO wpo)
{
    if (wpo.num_outer_sched_pred) {
        int num_components = vector_length(wpo.Cx);
        for (int i = 0; i < num_components; i++) {
            free(wpo.num_outer_sched_pred[i]);
        }
        free(wpo.num_outer_sched_pred);
    }

    graph_delete(wpo.wpo);

    free(wpo.num_sched_pred);
    free(wpo.node_to_component);

    if (wpo.Cx) {
        for (int i = 0; i < vector_length(wpo.Cx); i++) {
            C* comp = vector_get(wpo.Cx, i);
            if (comp->components) {
                vector_delete(comp->components);
            }
        }
        vector_delete(wpo.Cx);
    }

    vector_delete(wpo.heads);
    vector_delete(wpo.exits);
}
