#ifndef SCC_H
#define SCC_H

#include "graph.h"

typedef struct {
    int comp_count;
    Vector** components; // sizeof(Vector*) * num_block
    int* comp_id;
} SCC;

SCC* scc_build(Graph* graph);
void scc_print(SCC* scc);

#endif
