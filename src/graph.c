#include "graph.h"
#include "log.h"

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
        if (parent_graph->not_valid[index]) {
            continue;
        }

        map[index] = i;
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

    graph->not_valid = (uint8_t*)calloc(vector_length(graph->nodes), sizeof(uint8_t));

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
