#include "testCaseCorpus.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#define CORPUS_INITIAL_CAPACITY  1000000UL  // 1 million entries; adjust if needed

// Single global lock protecting all Corpus mutations and reads.
// If you ever want per-corpus locks, move this into the Corpus struct instead.
static pthread_mutex_t corpus_lock = PTHREAD_MUTEX_INITIALIZER;

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
    tc->in_corpus = false;

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

    memset(child->coverage_bitmap, 0, parent->cov_bytes);
    child->cov_bytes = parent->cov_bytes;

    child->fuzz_count = 0;

    return child;
}

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

    // In normal usage, you should only call this when no other threads
    // are touching the corpus anymore.
    pthread_mutex_lock(&corpus_lock);

    size_t n = atomic_load(&corpus->size);
    for (size_t i = 0; i < n; i++) {
        testcase_free(corpus->items[i]);
    }

    pthread_mutex_unlock(&corpus_lock);

    free(corpus->items);
    free(corpus);
}

void corpus_add(Corpus* corpus, TestCase* tc)
{
    if (!corpus || !tc) {
        return;
    }

    pthread_mutex_lock(&corpus_lock);

    size_t idx = atomic_load_explicit(&corpus->size, memory_order_relaxed);
    if (idx >= corpus->capacity) {
        // Corpus full → drop this testcase (or handle as you like)
        pthread_mutex_unlock(&corpus_lock);
        testcase_free(tc);
        return;
    }

    // Write the pointer first, then publish the updated size.
    corpus->items[idx] = tc;
    atomic_store_explicit(&corpus->size, idx + 1, memory_order_release);
    tc->in_corpus = true;
    pthread_mutex_unlock(&corpus_lock);
}

size_t corpus_size(Corpus* corpus)
{
    if (!corpus) return 0;

    pthread_mutex_lock(&corpus_lock);
    size_t n = atomic_load_explicit(&corpus->size, memory_order_acquire);
    pthread_mutex_unlock(&corpus_lock);

    return n;
}

TestCase* corpus_get(Corpus* corpus, size_t idx)
{
    if (!corpus) return NULL;

    pthread_mutex_lock(&corpus_lock);

    size_t n = atomic_load_explicit(&corpus->size, memory_order_acquire);
    TestCase* tc = NULL;
    if (idx < n) {
        tc = corpus->items[idx];
    }

    pthread_mutex_unlock(&corpus_lock);

    return tc;
}
