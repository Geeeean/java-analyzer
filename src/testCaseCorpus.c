#include "../include/testCaseCorpus.h"

Vector* corpus_init() {
  return vector_new(sizeof(TestCase*));
}
void corpus_free(Vector* corpus) {
  if (!corpus) {
    return;
  }

  for (size_t i = 0; i < vector_length(corpus); i++) {
    TestCase* tc = *(TestCase**)vector_get(corpus, i);
    testcase_free(tc);
  }
  vector_delete(corpus);
}

TestCase* create_testCase(const uint8_t* data,
                             size_t len,
                             const uint8_t* cov,
                             size_t cov_bytes) {
  TestCase* tc = malloc(sizeof(TestCase));

  if (!tc) {
    return NULL;
  }


  tc -> data = malloc(len);
  memcpy( tc -> data, data, len);
  tc -> len = len;

  tc -> coverage_bitmap = malloc(cov_bytes);
  memcpy(tc -> coverage_bitmap, cov, cov_bytes);
  tc -> cov_bytes = cov_bytes;

  tc -> fuzz_count = 0;

  return tc;
}

void testcase_free(TestCase* tc) {
  if (!tc) {
    return;
  }
  free(tc -> data);
  free(tc -> coverage_bitmap);
  free(tc);

}

void corpus_add(Vector* corpus, TestCase* tc) {
  if (!corpus || !tc) {
    return;
  }
  vector_push(corpus, &tc);

}

TestCase* corpus_choose(Vector* corpus, size_t* index) {
  if (!corpus || !index) {
    return NULL;
  }

  TestCase* tc = *(TestCase**)vector_get(corpus, *index);
  *index = (*index + 1) % vector_length(corpus);

  return tc;
}

TestCase* testCase_copy(TestCase* parent) {
  if (!parent) {
    return NULL;
  }
  TestCase* child = malloc(sizeof (TestCase));
  child -> data = malloc(parent -> len);
  memcpy( child -> data, parent -> data, parent -> len);
  child -> len = parent -> len;

  child -> coverage_bitmap = malloc(parent-> cov_bytes);
  memset(child -> coverage_bitmap, 0, parent -> cov_bytes);
  child -> cov_bytes = parent -> cov_bytes;

  child -> fuzz_count = 0;

  return child;
}