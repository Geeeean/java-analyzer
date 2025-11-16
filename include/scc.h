#ifndef SCC_H
#define SCC_H

#include "cfg.h"

typedef struct SCC SCC;

typedef struct {
    SCC* scc;
    Vector** comp_succ; // Vector<int> [component count]
} CondensedSCC;

CondensedSCC* cscc_build(Cfg* cfg);

#endif
