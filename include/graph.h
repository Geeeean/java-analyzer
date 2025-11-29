#ifndef GRAPH_H
#define GRAPH_H

#include "cfg.h"
#include "ir_function.h"
#include "vector.h"

#include <stdint.h>

typedef struct {
    Vector* successors; // Vector<int>
} Node;

typedef struct {
    Vector* nodes; // Vector<Node>
    uint8_t* not_valid;
} Graph;

typedef struct {
    Vector* nodes; // Vector<int>
    Vector* edges; // Vector<Pair>
} GraphMathRepr;

Graph* graph_from_cfg(Cfg* cfg);
Graph* graph_from_component(Graph* parent_graph, Vector* component, int* map);
Node* graph_get_node_by_id(Graph* graph, int id);

Graph* graph_from_graph_math_repr(GraphMathRepr* graph_mr);
GraphMathRepr* graph_math_repr_from_graph(Graph* graph);

Graph* graph_create_test_2_nodes();
Graph* graph_create_test_4_nodes();
Graph* graph_create_test_8_nodes();

void graph_print(Graph* graph);
void graph_delete(Graph* graph);

#endif
