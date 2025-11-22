// Source = https://clang.llvm.org/docs/SanitizerCoverage.html
#include "fuzzer.h"
#include "testCaseCorpus.h"
#include "vector.h"
#include "coverage.h"



Fuzzer* fuzzer_init(size_t instruction_count) {
  Fuzzer* f = malloc(sizeof(Fuzzer));
  f -> instruction_count = instruction_count;
  f -> cov_bytes = (instruction_count/7) + 8;
  f -> corpus = corpus_init();
  f -> corpus_index = 0;

  uint8_t empty[1] = {0};
  uint8_t* empty_cov = calloc(1, f-> cov_bytes);

  TestCase* seed = create_testCase(empty, 1, empty_cov, f->cov_bytes);

  corpus_add(f->corpus, seed);

  return f;

}

void fuzzer_run(Fuzzer* f) {
  while(not_done) {

    TestCase* parent = corpus_choose(f -> corpus, &f -> corpus_index);

    TestCase* child = testCase_copy(parent);

    child = mutate(child);

    if parse(child) = fail {
        continue;
      }

    interpreter_run(child);

    int new_bits = check_bits(child->coverage_bitmap, child -> cov_bytes);

    if (new_bits > 0) {
        corpus_add(f->corpus, child);
        global_cov_update(child->coverage_bitmap);
      }


  }
}

TestCase* mutate(TestCase* tc) {
  if (!tc) {
    return NULL;
  }

  return tc;
}



