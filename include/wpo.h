#ifndef WPO_H
#define WPO_H

#include "graph.h"

typedef struct WPOComponent WPOComponent;

typedef struct {
    Vector* components; // Vector<int>
} C;

typedef struct {
    Graph* wpo;
    int* num_sched_pred;
    int** num_outer_sched_pred;
    int* node_to_component;
    Vector* Cx; // Vector<C>
    Vector* heads; // Vector<int>
    Vector* exits;
} WPO;

int wpo_construct_aux(Graph* graph, WPO* wpo);
WPOComponent wpo_construct(GraphMathRepr* graph_mr, int* exit_index, Vector* Cx, Vector* heads, Vector* exits);
WPOComponent sccWPO(GraphMathRepr* graph, int* exit_index, Vector* Cx, Vector* heads, Vector* exits);

#endif
