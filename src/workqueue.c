#include "workqueue.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WORKQUEUE_EXTRA_CAPACITY 65536

void workqueue_init(WorkQueue* q, Corpus* corpus)
{
    assert(q != NULL);
    assert(corpus != NULL);

    size_t n = corpus_size(corpus);

    q->capacity = n + WORKQUEUE_EXTRA_CAPACITY;
    q->items = malloc(sizeof(TestCase*) * q->capacity);

    if (!q->items) {
        q->capacity = 0;
        q->initial_count = 0;
        atomic_store(&q->next_parent, 0);
        pthread_mutex_init(&q->child_lock, NULL);
        q->child_start = 0;
        q->child_end = 0;
        return;
    }

    q->initial_count = n;
    atomic_store(&q->next_parent, 0);

    pthread_mutex_init(&q->child_lock, NULL);

    q->child_start = n;
    q->child_end   = n;

    for (size_t i = 0; i < n; i++)
        q->items[i] = corpus_get(corpus, i);
}

void workqueue_destroy(WorkQueue* q)
{
    if (!q) return;

    pthread_mutex_destroy(&q->child_lock);

    if (q->items)
        free(q->items);

    q->items = NULL;
    q->capacity = 0;
    q->initial_count = 0;
    q->child_start = q->child_end = 0;
}

static void compact_children(WorkQueue* q)
{
    size_t live = q->child_end - q->child_start;
    if (live == 0) {
        q->child_start = q->child_end = q->initial_count;
        return;
    }

    if (q->child_start == q->initial_count)
        return;

    memmove(&q->items[q->initial_count],
            &q->items[q->child_start],
            live * sizeof(TestCase*));

    q->child_start = q->initial_count;
    q->child_end   = q->initial_count + live;
}

void workqueue_push(WorkQueue* q, TestCase* tc)
{
    if (!q || !tc || q->capacity == 0) return;

    pthread_mutex_lock(&q->child_lock);

    // If we need more space, compact first
    if (q->child_end >= q->capacity) {
        compact_children(q);

        if (q->child_end >= q->capacity) {
            // Still full => drop (or log)
            pthread_mutex_unlock(&q->child_lock);
            return;
        }
    }

    q->items[q->child_end++] = tc;

    pthread_mutex_unlock(&q->child_lock);
}

TestCase* workqueue_pop(WorkQueue* q)
{
    if (!q || q->capacity == 0)
        return NULL;

    size_t idx = atomic_fetch_add(&q->next_parent, 1);

    if (idx < q->initial_count) {
        TestCase* tc = q->items[idx];
        return tc;
    }

    pthread_mutex_lock(&q->child_lock);

    if (q->child_start >= q->child_end) {
        pthread_mutex_unlock(&q->child_lock);
        return NULL;
    }

    TestCase* tc = q->items[q->child_start++];

    if (q->child_start >= q->child_end)
        q->child_start = q->child_end = q->initial_count;

    pthread_mutex_unlock(&q->child_lock);
    return tc;
}
