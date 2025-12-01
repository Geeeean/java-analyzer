#ifndef ITGRAPH_H
#define ITGRAPH_H

#include "vector.h"
#include "method.h"
#include "config.h"

Vector* resolve_all_methods_reachable_from(const Method* root,
                                           const Config* cfg);

#endif