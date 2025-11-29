#ifndef SCC_H
#define SCC_H

#include "graph.h"

typedef struct {
    int comp_count;
    Vector** components; // sizeof(Vector*) * num_block
    int* comp_id;
    int num_nodes;
} SCC;

SCC* scc_build(Graph* graph);
void scc_print(SCC* scc);
void scc_delete(SCC* scc);

#endif
