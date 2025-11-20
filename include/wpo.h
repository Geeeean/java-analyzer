#ifndef WPO_H
#define WPO_H

#include "graph.h"

typedef struct WPOComponent WPOComponent;

typedef struct {
} WPO;

Graph* wpo_construct_aux(Graph* graph);
WPOComponent wpo_construct(GraphMathRepr* graph_mr, int* exit_index);
WPOComponent sccWPO(GraphMathRepr* graph, int* exit_index);

#endif
