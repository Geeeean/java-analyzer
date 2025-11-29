#ifndef FUZZER_H
#define FUZZER_H

#include "cli.h"
#include "interpreter.h"
#include "method.h"
#include "testCaseCorpus.h"
#include "vector.h"

#include <stdbool.h>

typedef struct {
  Corpus* corpus;
  size_t corpus_index;
  size_t cov_bytes;
  size_t instruction_count;
} Fuzzer;


Fuzzer* fuzzer_init(size_t instruction_count,
                     Vector* arg_types);
void    fuzzer_free(Fuzzer* f);

Vector* fuzzer_run_single(Fuzzer* f,
                          const Method* method,
                          const Config* config,
                          const Options* opts,
                          Vector* arg_types);
/**
 * Single-thread fuzz loop. Safe to call from multiple threads in parallel,
 * as long as they share the same Fuzzer*.
 */
Vector* fuzzer_run_thread(Fuzzer* f,
                          const Method* method,
                          const Config* config,
                          const Options* options,
                          Vector* arg_types);

/**
 * OpenMP parallel driver. Spawns `thread_count` threads that each run
 * `fuzzer_run_thread` and merges their interesting testcases.
 */
Vector* fuzzer_run_parallel(Fuzzer* f,
                            const Method* method,
                            const Config* config,
                            const Options* opts,
                            Vector* arg_types,
                            int thread_count);

TestCase* mutate(TestCase* tc, Vector* arg_types);

bool parse (const uint8_t* data,
            size_t length,
            Vector* arg_types,
            Options* out_opts);

Vector* fuzzer_run_until_complete(Fuzzer* f,
                                  const Method* method,
                                  const Config* config,
                                  const Options* opts,
                                  Vector* arg_types,
                                  int thread_count);

#endif