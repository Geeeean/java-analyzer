#ifndef SYNTAX_H
#define SYNTAX_H

#include "method.h"
#include "config.h"
#include "tree_sitter/api.h"

int get_method_node(method* m, config * cfg, TSNode* node);

#endif
