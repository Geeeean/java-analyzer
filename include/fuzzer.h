#ifndef FUZZER_H
#define FUZZER_H

#include "testCaseCorpus.h"
#include "vector.h"
#include <stdint.h>
#include <string.h>

typedef struct {
  Vector* corpus;
  size_t corpus_index;

  size_t cov_bytes;
  size_t instruction_count;
} Fuzzer;


Fuzzer* fuzzer_init(size_t instruction_count);

void fuzzer_run();

TestCase* mutate(TestCase* tc);

#endif