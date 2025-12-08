#ifndef FUZZER_H
#define FUZZER_H

#include "cli.h"
#include "interpreter_fuzz.h"
#include "interpreter_abstract.h"
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

Vector* fuzzer_run_parallel(Fuzzer* f,
                            const Method* method,
                            const Config* config,
                            const Options* opts,
                            Vector* arg_types,
                            int thread_count);

TestCase* mutate(TestCase* tc, Vector* arg_types);

Vector* fuzzer_run_until_complete(Fuzzer* f,
                                  const Method* method,
                                  const Config* config,
                                  const Options* opts,
                                  Vector* arg_types,
                                  int thread_count,
                                  AbstractResult* precomputed_abs);

uint8_t* fuzzer_make_seed(Vector* arg_types, size_t* out_len);

#endif
