#include "cli.h"
#include "config.h"
#include "info.h"
#include "interpreter.h"
#include "method.h"
#include "syntax.h"

#include "tree_sitter/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    int result = 0;
    Config* cfg = NULL;
    Method* m = NULL;
    TSTree* tree = NULL;

    /*** OPTIONS ***/
    options opts;
    if (parse_args(argc, (const char**)argv, &opts) > 0) {
        fprintf(stderr, "Correct usage: ./analyzer <options> <methodid>\n");
        result = 1;
        goto cleanup;
    }

    /*** CONFIG ***/
    cfg = config_load();
    if (!cfg) {
        fprintf(stderr, "Config file is wrongly formatted or not exist\n");
        result = 2;
        goto cleanup;
    }

    if (opts.info) {
        info_print(cfg);
        goto cleanup;
    }

    /*** METHOD ***/
    m = method_create(opts.method_id);
    if (!m) {
        fprintf(stderr, "Methodid is not valid\n");
        result = 3;
        goto cleanup;
    }

    /*** SYNTAX TREE ***/
    tree = build_syntax_tree(m, cfg);
    if (!tree) {
        fprintf(stderr, "Error while parsing code\n");
        result = 4;
        goto cleanup;
    }

    TSNode node;
    if (get_method_node(tree, &node)) {
        fprintf(stderr, "Error while getting method node in AST\n");
        result = 5;
        goto cleanup;
    }

    /*** INTERPRETER ***/
    InstructionTable* inst_table = build_instruction_table(m, cfg);

cleanup:
    ts_tree_delete(tree);
    method_delete(m);
    config_delete(cfg);
    delete_options(&opts);

    return result;
}
