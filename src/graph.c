#include "graph.h"
#include "log.h"
#include "pair.h"
#include <string.h>

#define ADD_SUCCESSOR(node_ptr, target_id)        \
    do {                                          \
        int id = (target_id);                     \
        vector_push((node_ptr)->successors, &id); \
    } while (0)

Graph* graph_from_cfg(Cfg* cfg)
{
    Graph* graph = malloc(sizeof(Graph));
    if (!graph) {
        goto cleanup;
    }

    graph->nodes = vector_new(sizeof(Node));

    for (int i = 0; i < vector_length(cfg->blocks); i++) {
        BasicBlock* block = *(BasicBlock**)vector_get(cfg->blocks, i);
        Node node = { .successors = vector_new(sizeof(int)) };

        for (int j = 0; j < vector_length(block->successors); j++) {
            BasicBlock* successor = *(BasicBlock**)vector_get(block->successors, j);
            int successor_id = successor->id;

            vector_push(node.successors, &successor_id);
        }

        vector_delete(block->successors);
        vector_push(graph->nodes, &node);
    }

    graph->not_valid = calloc(vector_length(graph->nodes), sizeof(uint8_t));

cleanup:
    LOG_ERROR("TODO graph free on error");
    return graph;
}

Graph* graph_from_component(Graph* parent_graph, Vector* component, int* map)
{
    Graph* graph = malloc(sizeof(Graph));
    if (!graph) {
        goto cleanup;
    }
    graph->nodes = vector_new(sizeof(Node));

    for (int i = 0; i < vector_length(parent_graph->nodes); i++) {
        map[i] = -1;
    }

    for (int i = 0; i < vector_length(component); i++) {
        int index = *(int*)vector_get(component, i);

        map[i] = index;
    }

    for (int i = 0; i < vector_length(component); i++) {
        Node new_node = { .successors = vector_new(sizeof(int)) };

        int index = *(int*)vector_get(component, i);
        Node* node = vector_get(parent_graph->nodes, index);
        for (int j = 0; j < vector_length(node->successors); j++) {
            int successor_index = *(int*)vector_get(node->successors, j);
            if (parent_graph->not_valid[successor_index]) {
                continue;
            }
            if (map[successor_index] >= 0) {
                vector_push(new_node.successors, &map[successor_index]);
            }
        }

        vector_push(graph->nodes, &new_node);
    }

    graph->not_valid = calloc(vector_length(graph->nodes), sizeof(uint8_t));

cleanup:
    return graph;
}

Graph* graph_create_test_2_nodes()
{
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (!graph)
        return NULL;

    graph->nodes = vector_new(sizeof(Node));
    if (!graph->nodes) {
        free(graph);
        return NULL;
    }

    for (int i = 0; i < 2; i++) {
        Node node = { .successors = vector_new(sizeof(int)) };
        vector_push(graph->nodes, &node);
    }

    Node* n0 = (Node*)vector_get(graph->nodes, 0);
    ADD_SUCCESSOR(n0, 1);

    Node* n1 = (Node*)vector_get(graph->nodes, 1);
    ADD_SUCCESSOR(n1, 0);

    graph->not_valid = calloc(vector_length(graph->nodes), sizeof(uint8_t));

    return graph;
}

Graph* graph_create_test_4_nodes()
{
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (!graph)
        return NULL;

    graph->nodes = vector_new(sizeof(Node));
    if (!graph->nodes) {
        free(graph);
        return NULL;
    }

    for (int i = 0; i < 4; i++) {
        Node node = { .successors = vector_new(sizeof(int)) };
        vector_push(graph->nodes, &node);
    }

    Node* n0 = (Node*)vector_get(graph->nodes, 0);
    ADD_SUCCESSOR(n0, 1);

    Node* n1 = (Node*)vector_get(graph->nodes, 1);
    ADD_SUCCESSOR(n1, 2);

    Node* n2 = (Node*)vector_get(graph->nodes, 2);
    ADD_SUCCESSOR(n2, 3);
    ADD_SUCCESSOR(n2, 1);

    Node* n3 = (Node*)vector_get(graph->nodes, 3);
    ADD_SUCCESSOR(n3, 0);

    graph->not_valid = calloc(vector_length(graph->nodes), sizeof(uint8_t));

    return graph;
}

Graph* graph_create_test_8_nodes()
{
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (!graph)
        return NULL;

    graph->nodes = vector_new(sizeof(Node));
    if (!graph->nodes) {
        free(graph);
        return NULL;
    }

    for (int i = 0; i < 8; i++) {
        Node node = { .successors = vector_new(sizeof(int)) };
        vector_push(graph->nodes, &node);
    }

    Node* n0 = (Node*)vector_get(graph->nodes, 0);
    ADD_SUCCESSOR(n0, 1);
    ADD_SUCCESSOR(n0, 4);

    Node* n1 = (Node*)vector_get(graph->nodes, 1);
    ADD_SUCCESSOR(n1, 2);

    Node* n2 = (Node*)vector_get(graph->nodes, 2);
    ADD_SUCCESSOR(n2, 1);
    ADD_SUCCESSOR(n2, 3);

    Node* n3 = (Node*)vector_get(graph->nodes, 3);
    ADD_SUCCESSOR(n3, 3);

    Node* n4 = (Node*)vector_get(graph->nodes, 4);
    ADD_SUCCESSOR(n4, 5);

    Node* n5 = (Node*)vector_get(graph->nodes, 5);
    ADD_SUCCESSOR(n5, 6);

    Node* n6 = (Node*)vector_get(graph->nodes, 6);
    ADD_SUCCESSOR(n6, 5);
    ADD_SUCCESSOR(n6, 2);
    ADD_SUCCESSOR(n6, 7);

    Node* n7 = (Node*)vector_get(graph->nodes, 7);
    ADD_SUCCESSOR(n7, 4);
    ADD_SUCCESSOR(n7, 3);

    graph->not_valid = calloc(vector_length(graph->nodes), sizeof(uint8_t));

    return graph;
}

void graph_print(Graph* graph)
{
    if (!graph || !graph->nodes) {
        LOG_INFO("Graph is NULL or empty.");
        return;
    }

    int num_nodes = vector_length(graph->nodes);

    LOG_INFO("--- Graph Structure (Total Nodes: %d) ---", num_nodes);

    for (int i = 0; i < num_nodes; i++) {
        Node* node = (Node*)vector_get(graph->nodes, i);

        if (graph->not_valid && graph->not_valid[i]) {
            LOG_INFO("Node %d: [INVALID/REMOVED]", i);
            continue;
        }

        LOG_INFO("Node %d ->", i);

        if (node->successors && vector_length(node->successors) > 0) {

            for (int j = 0; j < vector_length(node->successors); j++) {
                int successor_id = *(int*)vector_get(node->successors, j);

                LOG_INFO("  -> %d", successor_id);
            }
        } else {
            LOG_INFO("  -> <NONE>");
        }
    }
    LOG_INFO("-------------------------------------");
}

GraphMathRepr* graph_math_repr_from_graph(Graph* graph)
{
    GraphMathRepr* graph_mr = malloc(sizeof(GraphMathRepr));
    graph_mr->nodes = vector_new(sizeof(int));
    graph_mr->edges = vector_new(sizeof(Pair));

    for (int i = 0; i < vector_length(graph->nodes); i++) {
        vector_push(graph_mr->nodes, &i);
        Node* node = (Node*)vector_get(graph->nodes, i);
        for (int j = 0; j < vector_length(node->successors); j++) {
            Pair edge = {
                .first = i,
                .second = *(int*)vector_get(node->successors, j)
            };

            vector_push(graph_mr->edges, &edge);
        }
    }

    return graph_mr;
}

int get_max_id(Vector* nodes_vector)
{
    int max_id = -1;
    for (int i = 0; i < vector_length(nodes_vector); i++) {
        int id = *(int*)vector_get(nodes_vector, i);
        if (id > max_id) {
            max_id = id;
        }
    }
    return max_id;
}

Graph* graph_from_graph_math_repr(GraphMathRepr* graph_mr)
{
    if (!graph_mr || vector_length(graph_mr->nodes) == 0) {
        return NULL;
    }

    int max_id = get_max_id(graph_mr->nodes);
    int num_dense_nodes = max_id + 1;

    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (!graph)
        return NULL;

    graph->nodes = vector_new(sizeof(Node));
    if (!graph->nodes) {
        free(graph);
        return NULL;
    }

    graph->not_valid = (uint8_t*)malloc(num_dense_nodes * sizeof(uint8_t));
    if (!graph->not_valid) {
        return NULL;
    }

    memset(graph->not_valid, 1, num_dense_nodes * sizeof(uint8_t));

    for (int i = 0; i < num_dense_nodes; i++) {
        Node node = { .successors = NULL };
        vector_push(graph->nodes, &node);
    }

    for (int i = 0; i < vector_length(graph_mr->nodes); i++) {
        int sparse_id = *(int*)vector_get(graph_mr->nodes, i);

        graph->not_valid[sparse_id] = 0;
        Node* node_ptr = (Node*)vector_get(graph->nodes, sparse_id);
        node_ptr->successors = vector_new(sizeof(int));
    }

    for (int i = 0; i < vector_length(graph_mr->edges); i++) {
        Pair* edge = (Pair*)vector_get(graph_mr->edges, i);

        int source_id = edge->first;
        int target_id = edge->second;

        if (source_id < 0 || source_id >= num_dense_nodes) {
            continue;
        }

        if (graph->not_valid[source_id] == 0) {
            Node* source_node = (Node*)vector_get(graph->nodes, source_id);

            vector_push(source_node->successors, &target_id);
        }
    }

    return graph;
}
