#include "testCaseCorpus.h"

#include <stdlib.h>
#include <string.h>

#define CORPUS_INITIAL_CAPACITY  1000000UL  // 1 million entries; adjust if needed


// -----------------------------
// TestCase helpers
// -----------------------------

uint64_t testcase_hash(const TestCase *tc)
{
    uint64_t h = 0xcbf29ce484222325ULL;

    for (size_t i = 0; i < tc->len; i++) {
        h ^= tc->data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

TestCase* create_testCase(const uint8_t* data,
                          size_t len,
                          const uint8_t* cov,
                          size_t cov_bytes)
{
    TestCase* tc = malloc(sizeof(TestCase));
    if (!tc) {
        return NULL;
    }

    tc->data = malloc(len);
    if (!tc->data) {
        free(tc);
        return NULL;
    }
    memcpy(tc->data, data, len);
    tc->len = len;

    tc->coverage_bitmap = malloc(cov_bytes);
    if (!tc->coverage_bitmap) {
        free(tc->data);
        free(tc);
        return NULL;
    }
    memcpy(tc->coverage_bitmap, cov, cov_bytes);
    tc->cov_bytes = cov_bytes;

    tc->fuzz_count = 0;

    return tc;
}

void testcase_free(TestCase* tc) {
    if (!tc) {
        return;
    }
    free(tc->data);
    free(tc->coverage_bitmap);
    free(tc);
}

TestCase* testCase_copy(TestCase* parent) {
    if (!parent) {
        return NULL;
    }

    TestCase* child = malloc(sizeof(TestCase));
    if (!child) {
        return NULL;
    }

    child->data = malloc(parent->len);
    if (!child->data) {
        free(child);
        return NULL;
    }
    memcpy(child->data, parent->data, parent->len);
    child->len = parent->len;

    child->coverage_bitmap = malloc(parent->cov_bytes);
    if (!child->coverage_bitmap) {
        free(child->data);
        free(child);
        return NULL;
    }

    // child’s coverage bitmap starts at 0 (fresh execution)
    memset(child->coverage_bitmap, 0, parent->cov_bytes);
    child->cov_bytes = parent->cov_bytes;

    child->fuzz_count = 0;

    return child;
}

// -----------------------------
// Corpus implementation
// -----------------------------

Corpus* corpus_init(void)
{
    Corpus* c = malloc(sizeof(Corpus));
    if (!c) {
        return NULL;
    }

    c->capacity = CORPUS_INITIAL_CAPACITY;
    c->items = calloc(c->capacity, sizeof(TestCase*));
    if (!c->items) {
        free(c);
        return NULL;
    }

    atomic_store(&c->size, (size_t)0);
    return c;
}

void corpus_free(Corpus* corpus)
{
    if (!corpus) {
        return;
    }

    size_t n = atomic_load(&corpus->size);
    for (size_t i = 0; i < n; i++) {
        testcase_free(corpus->items[i]);
    }

    free(corpus->items);
    free(corpus);
}

void corpus_add(Corpus* corpus, TestCase* tc)
{
    if (!corpus || !tc) {
        return;
    }

    size_t idx = atomic_fetch_add_explicit(
        &corpus->size,
        (size_t)1,
        memory_order_relaxed
    );

    if (idx >= corpus->capacity) {
        // Corpus full → drop this testcase (or handle as you like)
        // Clamp size so it doesn't keep growing
        atomic_store_explicit(&corpus->size, corpus->capacity, memory_order_relaxed);
        testcase_free(tc);
        return;
    }

    corpus->items[idx] = tc;
}

size_t corpus_size(Corpus* corpus)
{
    if (!corpus) return 0;
    return atomic_load_explicit(&corpus->size, memory_order_acquire);
}

TestCase* corpus_get(Corpus* corpus, size_t idx)
{
    if (!corpus) return NULL;

    size_t n = atomic_load_explicit(&corpus->size, memory_order_acquire);
    if (idx >= n) {
        return NULL;
    }

    return corpus->items[idx];
}
