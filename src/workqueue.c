// workqueue.c
#include "workqueue.h"

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define WORKQUEUE_EXTRA_CAPACITY 65536

static size_t next_power_of_two(size_t x)
{
    if (x < 2)
        return 2;

    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
#if SIZE_MAX > UINT32_MAX
    x |= x >> 32;
#endif
    x++;
    return x;
}

void workqueue_init(WorkQueue* q, Corpus* corpus)
{
    assert(q != NULL);
    assert(corpus != NULL);

    size_t n = corpus_size(corpus);

    size_t requested = n + WORKQUEUE_EXTRA_CAPACITY;
    size_t capacity  = next_power_of_two(requested);

    q->capacity = capacity;
    q->mask     = capacity - 1;
    q->buffer   = (WorkQueueCell*)malloc(sizeof(WorkQueueCell) * capacity);

    if (!q->buffer) {
        q->capacity    = 0;
        q->mask        = 0;
        q->enqueue_pos = 0;
        q->dequeue_pos = 0;
        return;
    }

    for (size_t i = 0; i < capacity; i++) {
        atomic_store_explicit(&q->buffer[i].sequence, i, memory_order_relaxed);
        q->buffer[i].data = NULL;
    }

    atomic_store_explicit(&q->enqueue_pos, 0, memory_order_relaxed);
    atomic_store_explicit(&q->dequeue_pos, 0, memory_order_relaxed);

    for (size_t i = 0; i < n; i++) {
        TestCase* tc = corpus_get(corpus, i);
        if (tc)
            workqueue_push(q, tc);
    }
}

void workqueue_destroy(WorkQueue* q)
{
    if (!q) return;

    if (q->buffer) {
        free(q->buffer);
        q->buffer = NULL;
    }

    q->capacity    = 0;
    q->mask        = 0;
    q->enqueue_pos = 0;
    q->dequeue_pos = 0;
}

void workqueue_push(WorkQueue* q, TestCase* tc)
{
    if (!q || !tc || q->capacity == 0)
        return;

    size_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
    WorkQueueCell* cell;
    for (;;) {
        cell = &q->buffer[pos & q->mask];
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;

        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &q->enqueue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (dif < 0) {
            return;
        } else {
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }

    cell->data = tc;
    // Mark cell as ready for consumers
    atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);
}
TestCase* workqueue_pop(WorkQueue* q)
{
    if (!q || q->capacity == 0)
        return NULL;

    size_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
    WorkQueueCell* cell;
    for (;;) {
        cell = &q->buffer[pos & q->mask];
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);

        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &q->dequeue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (dif < 0) {
            return NULL;
        } else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }

    TestCase* tc = cell->data;
    atomic_store_explicit(
            &cell->sequence,
            pos + q->mask + 1,
            memory_order_release);

    return tc;
}
