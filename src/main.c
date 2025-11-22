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
#include "coverage.h"

#include "tree_sitter/api.h"
#include "type.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Forward declarations */
void run_interpreter(const Method* m, Options opts, const Config* cfg);
void run_fuzzer(const Method* m, Options opts, const Config* cfg);
void run_base(Method* m, Options opts, const Config* cfg);

int main(int argc, char** argv) {
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
    tree = syntax_tree_build(m, cfg);
    if (!tree) {
        LOG_ERROR("While parsing code");
        result = 4;
        goto cleanup;
    }

    TSNode node;
    if (method_node_get(tree, &node)) {
        LOG_ERROR("Error while getting method node in AST");
        result = 5;
        goto cleanup;
    }

    /*** MODES ***/
    if (opts.interpreter_only) {
        run_interpreter(m, opts, cfg);
    } else if (opts.fuzzer) {
        run_fuzzer(m,opts,cfg);
    } else {
        run_base(m,opts,cfg);
    }

cleanup:
    ts_tree_delete(tree);
    method_delete(m);
    config_delete(cfg);
    options_cleanup(&opts);

    return result;
}

void run_base(Method* m, Options opts, const Config* cfg) {
    int run = 100;
        Outcome outcome = new_outcome();

        size_t instruction_count = interpreter_instruction_count(m, cfg);

        coverage_init(instruction_count);

#pragma omp parallel
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);

            unsigned int seed = (unsigned int)(
                ts.tv_nsec ^
                (ts.tv_sec << 16) ^
                omp_get_thread_num()
            );

            srand(seed);
            srandom(seed);

            Outcome private = new_outcome();
            uint8_t* thread_bitmap = coverage_create_thread_bitmap();

#pragma omp for
            for (int i = 0; i < run; i++) {
                char*start_params = NULL;

                Options local_opts = opts;
                local_opts.parameters = start_params;


                VMContext* vm_context = interpreter_setup(m, &local_opts, cfg, NULL);

                if (!vm_context) {
                    LOG_ERROR("Interpreter setup failed (null VMContext) in parallel run. Skipping iteration.");
                    continue;
                }
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


/* interpreter-only runner */
void run_interpreter(const Method* m, Options opts, const Config* cfg) {
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
    case RT_CANT_BUILD_FRAME:
    case RT_NULL_PARAMETERS:
    case RT_UNKNOWN_ERROR:
    default:
        LOG_ERROR("Error while executing interpreter: %d", interpreter_result);
        break;
    }

    /*add VMContext cleanup */
}

void run_fuzzer(const Method* m, Options opts, const Config* cfg) {

  int run = 50;

  Outcome outcome = new_outcome();

  size_t best_cov = 0;

  size_t instruction_count = interpreter_instruction_count(m, cfg);

  if (!coverage_init(instruction_count)) {
    LOG_ERROR("Coverage initialization failed");
    return;
  }

#pragma omp parallel
  {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    uint32_t seed = (ts.tv_nsec
                    ^ (ts.tv_sec << 16)
                    ^ omp_get_thread_num()
    );

    srand(seed);
    srandom(seed);

    Outcome private = new_outcome();

    uint8_t* thread_bitmap = coverage_create_thread_bitmap();

   #pragma omp for
    for (int i = 0; i < run; i++) {
      Vector* v = method_get_arguments_as_types(m);

      char* fuzz_params = fuzzer_random_parameters(v);

      vector_delete(v);

      Options local_opts = opts;

      local_opts.parameters = fuzz_params;

      coverage_reset_thread_bitmap(thread_bitmap);

      VMContext* vm_context = interpreter_setup(m, &local_opts, cfg, thread_bitmap);

      if (!vm_context) {
        LOG_ERROR("Interpreter setup failed during fuzzing");
        return;
      }

      RuntimeResult r = interpreter_run(vm_context);

      size_t new_cov = coverage_commit_thread(thread_bitmap);
      size_t global_cov = coverage_global_count();

    #pragma omp critical
      {
          if (new_cov > 0) {
            best_cov = global_cov;
            printf("[+] New coverage: %zu | Total %zu / %zu (%.2f%% | input= '%s'\n" ,
                new_cov,
                global_cov,
                instruction_count,
                instruction_count ? (global_cov * 100.0 / instruction_count) : 0.0,
                fuzz_params);
          }
      }

      switch (r) {
      case RT_OK: private.oc_ok = 100; break;
      case RT_DIVIDE_BY_ZERO: private.oc_divide_by_zero = 100; break;
      case RT_ASSERTION_ERR: private.oc_assertion_error = 100; break;
      case RT_INFINITE: private.oc_infinite_loop = 75; break;
      case RT_OUT_OF_BOUNDS: private.oc_out_of_bounds = 100; break;
      case RT_NULL_POINTER: private.oc_null_pointer = 100; break;
      default:
        LOG_ERROR("Interpreter error: %d", r);
        break;
      }

      free(fuzz_params);
    } // End of for-loop
  #pragma omp critical
    {
#define MERGE(field) if (private.field != 50) outcome.field = private.field;
      MERGE(oc_ok);
      MERGE(oc_divide_by_zero);
      MERGE(oc_assertion_error);
      MERGE(oc_infinite_loop);
      MERGE(oc_null_pointer);
      MERGE(oc_out_of_bounds);
#undef MERGE
    }
  } // End of parallel block
  print_outcome(outcome);

  printf("\n=== FINAL COVERAGE REPORT ===\n");
  printf("Instructions covered: %zu / %zu\n", best_cov, instruction_count);

  if (instruction_count > 0) {
    printf("Coverage: %.2f%%\n", (best_cov * 100.0) / instruction_count);
  }

  printf("Coverage bitmap (first 64 PCs): ");
  coverage_global_print(instruction_count);

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
