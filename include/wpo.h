#ifndef WPO_H
#define WPO_H

#include "graph.h"

typedef struct WPOComponent WPOComponent;

typedef struct {
} WPO;

void* wpo_construct_aux(Graph* graph);
WPOComponent wpo_construct(Graph* graph);

#endif
