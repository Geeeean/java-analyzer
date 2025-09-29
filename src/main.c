#include "cli.h"
#include "config.h"
#include "decompiled_parser.h"
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
    InstructionTable* instruction_table = NULL;

    /*** OPTIONS ***/
    Options opts;
    if (options_parse_args(argc, (const char**)argv, &opts) > 0) {
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
    tree = syntax_tree_build(m, cfg);
    if (!tree) {
        fprintf(stderr, "Error while parsing code\n");
        result = 4;
        goto cleanup;
    }

    TSNode node;
    if (method_node_get(tree, &node)) {
        fprintf(stderr, "Error while getting method node in AST\n");
        result = 5;
        goto cleanup;
    }

    /*** INTERPRETER ***/
    instruction_table = instruction_table_build(m, cfg);
    if (!instruction_table) {
        fprintf(stderr, "Error while building instruction table\n");
        result = 6;
        goto cleanup;
    }

    RuntimeResult interpreter_result = interpreter_run(instruction_table, m, opts.parameters);
    if (opts.interpreter_only) {
        switch (interpreter_result) {
        case RT_OK:
            printf("ok\n");
            break;
        case RT_DIVIDE_BY_ZERO:
            printf("divide by zero\n");
            break;
        case RT_ASSERTION_ERR:
            printf("assertion error\n");
            break;
        case RT_INFINITE:
            printf("*\n");
            break;
        case RT_CANT_BUILD_FRAME:
        case RT_NULL_PARAMETERS:
        case RT_UNKNOWN_ERROR:
        default:
            fprintf(stderr, "Error while executing interpreter: %d\n", interpreter_result);
        }
    }

cleanup:
    instruction_table_delete(instruction_table);
    ts_tree_delete(tree);
    method_delete(m);
    config_delete(cfg);
    options_cleanup(&opts);

    return result;
}


/*
 * TODO
 * 1 -> handle array instructions
 * 2 -> handle function calls
 * 3 -> go from interpreting to evaluation
 * 4 -> choose a project
 * 5 -> get 12
 */
