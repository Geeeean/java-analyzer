#ifndef GRAPH_H
#define GRAPH_H

#include "cfg.h"
#include "vector.h"

typedef struct {
    Vector* successors;
} Node;

typedef struct {
    Vector* nodes;
    uint8_t* not_valid;
} Graph;

Graph* graph_from_cfg(Cfg* cfg);
Graph* graph_from_component(Graph* parent_graph, Vector* component, int* map);
Node* graph_get_node_by_id(Graph* graph, int id);

Graph* graph_create_test_8_nodes();
void graph_print(Graph* graph);

#endif
