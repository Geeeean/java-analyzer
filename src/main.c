#include "cli.h"
#include "config.h"
#include "coverage.h"
#include "domain_interval.h"
#include "fuzzer.h"
#include "info.h"
#include "interpreter_abstract.h"
#include "interpreter_concrete.h"
#include "ir_program.h"
#include "log.h"
#include "method.h"
#include "outcome.h"
#include "vector.h"

#include "tree_sitter/api.h"
#include "utils.h"
#include "interval_testcase.h"
#include <omp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Forward declarations */
void run_interpreter(const Method* m, Options opts, const Config* cfg);
void run_fuzzer(const Method* m, Options opts, const Config* cfg);
void run_base(Method* m, Options opts, const Config* cfg);

int main(int argc, char** argv)
{
#ifdef DEBUG
    omp_set_num_threads(1);
#endif

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
        LOG_ERROR("Failed to load configuration. See detailed messages above for exact reason and location.");
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
    // tree = syntax_tree_build(m, cfg);
    // if (!tree) {
    //     LOG_ERROR("While parsing code");
    //     result = 4;
    //     goto cleanup;
    // }
    //
    // TSNode node;
    // if (method_node_get(tree, &node)) {
    //     LOG_ERROR("Error while getting method node in AST");
    //     result = 5;
    //     goto cleanup;
    // }

    /*** MODES ***/
    if (opts.interpreter_only) {
        run_interpreter(m, opts, cfg);
    } else if (opts.abstract_only) {
        AbstractContext* abstract_context = interpreter_abstract_setup(m, &opts, cfg);

        struct timeval start, end;
        gettimeofday(&start, NULL); // capture start time

        interpreter_abstract_run(abstract_context);

        gettimeofday(&end, NULL); // capture end time

        long seconds = end.tv_sec - start.tv_sec;
        long useconds = end.tv_usec - start.tv_usec;

        long total_microseconds = seconds * 1000000L + useconds;
        LOG_BENCHMARK("Elapsed time: %ld microseconds\n", total_microseconds);
    } else if (opts.fuzzer) {
        run_fuzzer(m, opts, cfg);
    } else {
        AbstractContext* abstract_context = interpreter_abstract_setup(m, &opts, cfg);
        AbstractResult abstract_result = interpreter_abstract_run(abstract_context);

#ifdef DEBUG
        for (int i = 0; i < abstract_result.num_locals; i++) {
            LOG_INFO("LOCAL[%d]", i);
            for (int j = 0; j < vector_length(abstract_result.results[i]); j++) {
                Interval* iv = vector_get(abstract_result.results[i], j);
                printf("[%d, %d] ", iv->lower, iv->upper);
            }
            printf("\n");
        }
#endif

        // TODO: use abstract result for starting the fuzzer
    }

cleanup:
    ir_program_delete();
    // ts_tree_delete(tree);
    method_delete(m);
    config_delete(cfg);
    options_cleanup(&opts);

    return result;
}

/* interpreter-only runner */
void run_interpreter(const Method* m, Options opts, const Config* cfg)
{
    VMContext* vm_context = interpreter_setup(m, &opts, cfg, NULL);
    if (!vm_context) {
        LOG_ERROR("Interpreter setup failed (null VMContext). See previous errors.");
        return;
    }

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
    case RT_UNKNOWN_ERROR:
    default:
        LOG_ERROR("Error while executing interpreter: %d", interpreter_result);
        break;
    }

    interpreter_free(vm_context);
}

void run_fuzzer(const Method* m, Options opts, const Config* cfg)
{


    size_t instruction_count = interpreter_instruction_count(m, cfg);

    if (!coverage_init(instruction_count)) {
        LOG_ERROR("Coverage init failed.");
        return;
    }

    Vector* arg_types = method_get_arguments_as_types(m);
    if (!arg_types) {
        LOG_ERROR("Argument types missing.");
        coverage_reset_all();
        return;
    }

    AbstractContext* abs_ctx = interpreter_abstract_setup(m, &opts, cfg);
    AbstractResult abs_result = interpreter_abstract_run(abs_ctx);

    Fuzzer* f = fuzzer_init(instruction_count, arg_types);
    if (!f) {
        LOG_ERROR("Fuzzer init failed.");
        vector_delete(arg_types);
        coverage_reset_all();
        return;
    }

    int thread_count = 1;
#ifdef _OPENMP
    int omp_threads = omp_get_max_threads();
    thread_count = omp_threads;
#endif

    Vector* interval_seeds = generate_interval_seeds(f, &abs_result, arg_types);

    printf("arg_types count = %zu\n", vector_length(arg_types));
    for (size_t i = 0; i < vector_length(arg_types); i++) {
        Type* t = *(Type**)vector_get(arg_types, i);
        printf("  arg %zu kind = %d\n", i, t->kind);
    }
    Vector* results = fuzzer_run_until_complete(f, m, cfg, &opts, arg_types, thread_count, interval_seeds);

    size_t covered = coverage_global_count();

    printf("\n=== FINAL COVERAGE REPORT ===\n");
    printf("Instructions covered: %zu / %zu\n", covered, instruction_count);

    fuzzer_free(f);
    vector_delete(arg_types);
    vector_delete(results);

    coverage_reset_all();
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
