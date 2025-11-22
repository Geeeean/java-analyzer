//
// Created by marti on 22/11/2025.
//
#ifndef TESTCASECORPUS_H
#define TESTCASECORPUS_H

#include "vector.h"
#include <stdint.h>
#include <string.h>

typedef struct {
  uint8_t* data;
  size_t   len;

  uint8_t* coverage_bitmap;
  size_t   cov_bytes;

  unsigned int fuzz_count;
} TestCase;

Vector* corpus_init(void);
void corpus_free(Vector* corpus);

TestCase* create_testCase(const uint8_t* data,
                             size_t len,
                             const uint8_t* cov,
                             size_t cov_bytes);

void testcase_free(TestCase* tc);

void corpus_add(Vector* corpus, TestCase* tc);

TestCase* corpus_choose(Vector* corpus, size_t* index);
#endif

TestCase* testCase_copy(TestCase* parent);