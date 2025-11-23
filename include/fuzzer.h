#ifndef FUZZER_H
#define FUZZER_H

#include "cli.h"
#include "method.h"
#include "testCaseCorpus.h"
#include "vector.h"

#include <stdbool.h>

typedef struct {
  Vector* corpus;
  size_t corpus_index;
  size_t cov_bytes;
  size_t instruction_count;
} Fuzzer;


Fuzzer* fuzzer_init(size_t instruction_count);

Vector* fuzzer_run(Fuzzer* f,
                const Method* method,
                const Config* config,
                const Options* options,
                Vector* arg_types);

void fuzzer_free(Fuzzer* f);

TestCase* mutate(TestCase* tc);

bool parse (const uint8_t* data,
            size_t length,
            Vector* arg_types,
            Options* out_opts);

#endif