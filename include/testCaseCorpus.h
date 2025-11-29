#ifndef TESTCASECORPUS_H
#define TESTCASECORPUS_H

#include "vector.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ------------------------------------
// TestCase
// ------------------------------------

typedef struct {
    uint8_t* data;
    size_t len;

    uint8_t* coverage_bitmap;
    size_t cov_bytes;

    unsigned int fuzz_count;
    bool in_corpus;
} TestCase;

typedef struct {
    TestCase** items;
    atomic_size_t size;
    size_t capacity;
    pthread_mutex_t resize_lock;
} Corpus;

// API
Corpus* corpus_init(void);
void corpus_free(Corpus* c);

void corpus_add(Corpus* c, TestCase* tc);
size_t corpus_size(Corpus* c);
TestCase* corpus_get(Corpus* c, size_t idx);

// TestCase helpers
TestCase* create_testCase(const uint8_t* data,
    size_t len,
    const uint8_t* cov,
    size_t cov_bytes);

void testcase_free(TestCase* tc);
TestCase* testCase_copy(TestCase* parent);
uint64_t testcase_hash(const TestCase* tc);

#endif
