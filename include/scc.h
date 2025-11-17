#ifndef SCC_H
#define SCC_H

#include "cfg.h"

typedef struct {
    int comp_count;
    Vector** components; // sizeof(Vector*) * num_block
    int* comp_id;
} SCC;

typedef struct {
    SCC* scc;
    Vector** comp_succ; // Vector<int> [component count]
    int* head; // [component count]
    uint8_t* is_loop; // [component count]
    int* exit_id; // [component count]
    int wpo_node_count;
    Vector** successors;
} CondensedSCC;

CondensedSCC* cscc_build(Cfg* cfg);

#endif
