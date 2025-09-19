#include "cli.h"
#include "config.h"
#include "info.h"
#include "method.h"
#include "syntax.h"

#include "tree_sitter/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    int result = 0;

    /*** OPTIONS ***/
    options opts;
    if (parse_args(argc, (const char**)argv, &opts) > 0) {
        fprintf(stderr, "Correct usage: ./analyzer <options> <methodid>\n");
        result = 1;
        goto cleanup;
    }

    /*** CONFIG ***/
    Config* cfg = config_load();
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
    Method* m = method_create(opts.method_id);
    if (!m) {
        fprintf(stderr, "Methodid is not valid\n");
        result = 3;
        goto cleanup;
    }

    /*** SYNTAX TREE ***/
    TSTree* tree = NULL;
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
    char* dec_json = method_read(m, cfg, SRC_DECOMPILED);
    printf("%s", dec_json);
    free(dec_json);

cleanup:
    ts_tree_delete(tree);
    method_delete(m);
    config_delete(cfg);
    delete_options(&opts);

    return result;
}
