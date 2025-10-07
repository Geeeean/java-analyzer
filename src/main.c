#include "cli.h"
#include "config.h"
#include "opcode.h"
#include "info.h"
#include "interpreter.h"
#include "log.h"
#include "method.h"
#include "outcome.h"
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
        LOG_ERROR("Correct usage: ./analyzer <options> <methodid>");
        result = 1;
        goto cleanup;
    }

    /*** CONFIG ***/
    cfg = config_load();
    if (!cfg) {
        LOG_ERROR("Config file is wrongly formatted or not exist");
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
        LOG_ERROR("Methodid is not valid");
        result = 3;
        goto cleanup;
    }

    /*** SYNTAX TREE ***/
    tree = syntax_tree_build(m, cfg);
    if (!tree) {
        LOG_ERROR("Error while parsing code");
        result = 4;
        goto cleanup;
    }

    TSNode node;
    if (method_node_get(tree, &node)) {
        LOG_ERROR("Error while getting method node in AST");
        result = 5;
        goto cleanup;
    }

    /*** INTERPRETER ***/
    CallStack* call_stack = interpreter_setup(m, &opts, cfg);
    LOG_DEBUG("CALL STACK CREATED: %p", call_stack);

    RuntimeResult interpreter_result = interpreter_run(call_stack, cfg);
    if (opts.interpreter_only) {
        switch (interpreter_result) {
        case RT_OK:
            print_interpreter_outcome(OC_OK);
            break;
        case RT_DIVIDE_BY_ZERO:
            print_interpreter_outcome(OC_DIVIDE_BY_ZERO);
            break;
        case RT_ASSERTION_ERR:
            print_interpreter_outcome(OC_ASSERTION_ERROR);
            break;
        case RT_INFINITE:
            print_interpreter_outcome(OC_INFINITE_LOOP);
            break;
        case RT_OUT_OF_BOUNDS:
            print_interpreter_outcome(OC_OUT_OF_BOUNDS);
            break;
        case RT_NULL_POINTER:
            print_interpreter_outcome(OC_NULL_POINTER);
            break;
        case RT_CANT_BUILD_FRAME:
        case RT_NULL_PARAMETERS:
        case RT_UNKNOWN_ERROR:
        default:
            LOG_ERROR("Error while executing interpreter: %d", interpreter_result);
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
 * 0 -> make dynamic array structure
 * 1 -> handle array instructions
 * 2 -> handle function calls
 * 3 -> go from interpreting to evaluation
 * 4 -> choose a project
 * 5 -> get 12
 */
