#ifndef SCC_H
#define SCC_H

#include "cfg.h"
#include "graph.h"

typedef struct {
    int comp_count;
    Vector** components; // sizeof(Vector*) * num_block
    int* comp_id;
} SCC;

void* wpo_construct_aux(Graph* graph);
SCC* scc_build(Graph* graph);
void scc_print(SCC* scc);

#endif
