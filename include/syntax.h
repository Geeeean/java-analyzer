#ifndef SYNTAX_H
#define SYNTAX_H

#include "config.h"
#include "method.h"

#include "tree_sitter/api.h"

TSTree* syntax_tree_build(Method* m, Config* cfg);
int method_node_get(TSTree* tree, TSNode* node);

#endif
