#include "wpo.h"
#include "cfg.h"
#include "common.h"
#include "log.h"
#include "scc.h"
#include "vector.h"

#include <limits.h>

struct WPOComponent {
    int nodes;
    int exits;
    Vector** scheduling_edges; // sizeof(Vector*) * (number of nodes)
    Vector** stabilizing_edges; // sizeof(Vector*) * (number of exits)
    int head;
    int exit_head;
};

static void print_edge_array(const char* name, int size, Vector** edge_array)
{
    if (size == 0) {
        LOG_INFO("  %s: <EMPTY>", name);
        return;
    }

    LOG_INFO("  --- %s (%d entries) ---", name, size);

    for (int i = 0; i < size; i++) {
        Vector* current_edges = edge_array[i];

        if (current_edges && vector_length(current_edges) > 0) {

            for (int j = 0; j < vector_length(current_edges); j++) {
                int target = *(int*)vector_get(current_edges, j);
                LOG_INFO("    Edge [%d] -> %d", i, target);
            }
        } else {
            LOG_INFO("    Edge [%d] -> <NONE>", i);
        }
    }
}

void wpo_component_print(WPOComponent* wpo)
{
    if (!wpo) {
        LOG_INFO("WPOComponent is NULL.");
        return;
    }

    LOG_INFO("==================================================");
    LOG_INFO(">>> WPO Component Analysis <<<");

    LOG_INFO("General Info:");
    LOG_INFO("  Total Nodes (V): %d", wpo->nodes);
    LOG_INFO("  Total Exits: %d", wpo->exits);
    LOG_INFO("  Head Node (h): %d", wpo->head);
    LOG_INFO("  Exit Head Node (xh): %d", wpo->exit_head);

    LOG_INFO("Scheduling Edges (->):");
    print_edge_array("Scheduling Edges", wpo->nodes, wpo->scheduling_edges);

    LOG_INFO("Stabilizing Edges (=>):");
    print_edge_array("Stabilizing Edges", wpo->exits, wpo->stabilizing_edges);

    LOG_INFO("==================================================");
}

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

static WPOComponent trivial_WPO()
{
    int head = 0;

    Vector** scheduling_edges = new_edges_vector(0);
    Vector** stabilizing_edges = new_edges_vector(0);

    return (WPOComponent) {
        .nodes = 1,
        .exits = 0,
        .scheduling_edges = scheduling_edges,
        .stabilizing_edges = stabilizing_edges,
        .head = head,
        .exit_head = head
    };
}

static WPOComponent self_loop_WPO()
{
    int head = 0;
    int exit_id = 1;

    Vector** scheduling_edges = new_edges_vector(1);
    vector_push(scheduling_edges[0], &exit_id);

    Vector** stabilizing_edges = new_edges_vector(1);
    vector_push(stabilizing_edges[0], &head);

    return (WPOComponent) {
        .nodes = 1,
        .exits = 1,
        .scheduling_edges = scheduling_edges,
        .stabilizing_edges = stabilizing_edges,
        .head = head,
        .exit_head = head
    };
}

static int
compute_b_len(Graph* graph, int head)
{
    graph_print(graph);

    int res = 0;
    for (int i = 0; i < vector_length(graph->nodes); i++) {
        Node* node = vector_get(graph->nodes, i);
        for (int j = 0; j < vector_length(node->successors); j++) {
            int successor_id = *(int*)vector_get(node->successors, j);
            if (head == successor_id) {
                res++;
                // break;
            }
        }
    }

    return res;
}
WPOComponent sccWPO(Graph* graph)
{
    LOG_INFO("B");
    int head = 0;

    // node that has an edge to the head
    int b_len = compute_b_len(graph, head);
    LOG_INFO("B LEN %d", b_len);

    if (b_len == 0) {
        return trivial_WPO();
    } else if (vector_length(graph->nodes) == 1) {
        return self_loop_WPO();
    }

    LOG_INFO("C");
    int exit = vector_length(graph->nodes);

    // V' = V ∪ {exit_h} ∖ {h}
    for (int i = 0; i < exit; i++) {
        Node* node = vector_get(graph->nodes, i);
        for (int j = 0; j < vector_length(node->successors); j++) {
            int successor_id = *(int*)vector_get(node->successors, j);
            if (head == successor_id) {
                vector_push(node->successors, &exit);
                break;
            }
        }
    }
    graph->not_valid[head] = 1;
    int* map = malloc(sizeof(int) * vector_length(graph->nodes));
    WPOComponent new_component = wpo_construct(graph);

    LOG_INFO("D");
    // exit from new component becomes exit+counter
    // the other nodes are mapped with map

    Vector** scheduling_edges = new_edges_vector(new_component.nodes + 1);
    Node* head_node = vector_get(graph->nodes, 0);
    vector_copy(scheduling_edges[0], head_node->successors);

    for (int i = 0; i < new_component.nodes; i++) {
        int og_index = map[i];
        Vector* new_scheduling_edges = new_component.scheduling_edges[i];
        for (int j = 0; j < vector_length(new_scheduling_edges); j++) {
            int edge = *(int*)vector_get(new_scheduling_edges, j);
            if (edge < new_component.nodes) {
                vector_push(scheduling_edges[og_index], &map[edge]);
            } else {
                int exit_normalized = edge - new_component.nodes;
                int new_exit = exit_normalized + exit + 1;
                vector_push(scheduling_edges[og_index], &new_exit);
            }
        }
    }

    LOG_INFO("E");
    Vector** stabilizing_edges = new_edges_vector(new_component.exits + 1);
    vector_push(stabilizing_edges[0], &head);
    for (int i = 0; i < new_component.exits; i++) {
        int og_index = i + 1;
        Vector* new_stabilizing_edges = new_component.stabilizing_edges[i];
        for (int j = 0; j < vector_length(new_stabilizing_edges); j++) {
            int edge = *(int*)vector_get(new_stabilizing_edges, j);
            vector_push(stabilizing_edges[og_index], &map[edge]);
        }
    }

    WPOComponent result = (WPOComponent) {
        .nodes = new_component.nodes + 1,
        .exits = new_component.exits + 1,
        .scheduling_edges = scheduling_edges,
        .stabilizing_edges = stabilizing_edges,
        .head = 0,
        .exit_head = exit
    };

cleanup:
    free(map);

    return result;
}

void* wpo_construct_aux(Graph* graph)
{
    WPOComponent result = wpo_construct(graph);
    return NULL;
}

WPOComponent wpo_construct(Graph* graph)
{
    LOG_INFO("A");
    SCC* scc = scc_build(graph);
    for (int i = 0; i < scc->comp_count; i++) {
        LOG_INFO("COMPONENT %d, len: %ld", i, vector_length(scc->components[i]));
    }
    WPOComponent* wpo_components = malloc(scc->comp_count * sizeof(WPOComponent));
    int** map = malloc(sizeof(int*) * scc->comp_count);

    int exit_base = vector_length(graph->nodes);
    Vector** scheduling_edges = new_edges_vector(vector_length(graph->nodes));

    int exit_nodes = 0;

    // processing
    for (int i = 0; i < scc->comp_count; i++) {
        LOG_INFO("PROCESSING COMP: %d", i);
        map[i] = malloc(sizeof(int) * vector_length(graph->nodes));
        Graph* component_graph = graph_from_component(graph, scc->components[i], map[i]);
        LOG_INFO("GRAPH OBTAINED");
        wpo_components[i] = sccWPO(component_graph);
        exit_nodes += wpo_components[i].exits;

        // wpo_component_print(&wpo_component);
    }

    LOG_INFO("CIAO");

    Vector** stabilizing_edges = new_edges_vector(exit_nodes);

    // mapping (fusion)
    for (int i = 0; i < scc->comp_count; i++) {
        LOG_INFO("COMPONENT %d", i);
        WPOComponent wpo_component = wpo_components[i];

        for (int j = 0; j < wpo_component.nodes; j++) {
            int og_index = map[i][j];
            Vector* new_scheduling_edges = wpo_component.scheduling_edges[i];
            for (int k = 0; k < vector_length(new_scheduling_edges); k++) {
                int successor = *(int*)vector_get(new_scheduling_edges, k);
                if (successor < wpo_component.nodes) {
                    int og_successor = map[i][successor];
                    vector_push(scheduling_edges[og_index], &og_successor);
                } else {
                    int exit_normalized = successor - wpo_component.nodes;
                    int og_exit = exit_normalized + exit_base;
                    vector_push(scheduling_edges[og_index], &og_exit);
                }
            }
        }

        for (int j = 0; j < wpo_component.exits; i++) {
            int og_index = exit_base - vector_length(graph->nodes);
            Vector* new_stabilizing_edges = wpo_component.stabilizing_edges[i];
            for (int k = 0; k < vector_length(new_stabilizing_edges); k++) {
                int edge = *(int*)vector_get(new_stabilizing_edges, k);
                vector_push(stabilizing_edges[og_index], &map[edge]);
            }
        }

        exit_base += wpo_component.exits;
    }
    LOG_INFO("H");

    WPOComponent result = (WPOComponent) {
        .nodes = vector_length(graph->nodes),
        .exits = exit_nodes,
        .scheduling_edges = scheduling_edges,
        .stabilizing_edges = stabilizing_edges,
        .head = -1,
        .exit_head = -1
    };
cleanup:

    return result;
}
