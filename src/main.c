#include "cli.h"
#include "config.h"
#include "fuzzer.h"
#include "info.h"
#include "interpreter.h"
#include "log.h"
#include "method.h"
#include "outcome.h"
#include "syntax.h"
#include "vector.h"

#include "tree_sitter/api.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    int result = 0;
    Config* cfg = NULL;
    Method* m = NULL;
    TSTree* tree = NULL;

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
    if (opts.interpreter_only) {
        VMContext* vm_context = interpreter_setup(m, &opts, cfg);
        RuntimeResult interpreter_result = interpreter_run(vm_context);

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
    } else {
        int run = 100;
        Outcome outcome = new_outcome();

#pragma omp parallel
        {
            Outcome private = new_outcome();

#pragma omp for
            for (int i = 0; i < run; i++) {
                if (!opts.interpreter_only) {
                    Vector* v = method_get_arguments_as_types(m);
                    opts.parameters = fuzzer_random_parameters(v);
                    vector_delete(v);
                }

                VMContext* vm_context = interpreter_setup(m, &opts, cfg);
                RuntimeResult interpreter_result = interpreter_run(vm_context);

                switch (interpreter_result) {
                case RT_OK:
                    private.oc_ok = 100;
                    break;
                case RT_DIVIDE_BY_ZERO:
                    private.oc_divide_by_zero = 100;
                    break;
                case RT_ASSERTION_ERR:
                    private.oc_assertion_error = 100;
                    break;
                case RT_INFINITE:
                    private.oc_infinite_loop = 75;
                    break;
                case RT_OUT_OF_BOUNDS:
                    private.oc_out_of_bounds = 100;
                    break;
                case RT_NULL_POINTER:
                    private.oc_null_pointer = 100;
                    break;
                case RT_CANT_BUILD_FRAME:
                case RT_NULL_PARAMETERS:
                case RT_UNKNOWN_ERROR:
                default:
                    LOG_ERROR("Error while executing interpreter: %d", interpreter_result);
                }
            }

#pragma omp critical
            {
                if (private.oc_assertion_error != 50) {
                    outcome.oc_assertion_error = private.oc_assertion_error;
                }
                if (private.oc_divide_by_zero != 50) {
                    outcome.oc_divide_by_zero = private.oc_divide_by_zero;
                }
                if (private.oc_infinite_loop != 50) {
                    outcome.oc_infinite_loop = private.oc_infinite_loop;
                }
                if (private.oc_null_pointer != 50) {
                    outcome.oc_null_pointer = private.oc_null_pointer;
                }
                if (private.oc_ok != 50) {
                    outcome.oc_ok = private.oc_ok;
                }
                if (private.oc_out_of_bounds != 50) {
                    outcome.oc_out_of_bounds = private.oc_out_of_bounds;
                }
            }
        }

        print_outcome(outcome);
    }

cleanup:
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
