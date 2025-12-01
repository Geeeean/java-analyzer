#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>

#include "testCaseCorpus.h"

typedef struct {
    TestCase** items;
    size_t capacity;

    size_t initial_count;
    atomic_size_t next_parent;

    pthread_mutex_t child_lock;
    size_t child_start;
    size_t child_end;
} WorkQueue;

void workqueue_init(WorkQueue* q, Corpus* corpus);
void workqueue_destroy(WorkQueue* q);
void workqueue_push(WorkQueue* q, TestCase* tc);
TestCase* workqueue_pop(WorkQueue* q);

#endif // WORKQUEUE_H
