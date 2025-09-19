#ifndef SYNTAX_H
#define SYNTAX_H

#include "config.h"
#include "method.h"
#include "tree_sitter/api.h"

TSTree* build_syntax_tree(Method* m, Config* cfg);
int get_method_node(TSTree* tree, TSNode* node);

#endif
