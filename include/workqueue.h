#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#pragma once

#include <stddef.h>
#include <stdatomic.h>

#include "testCaseCorpus.h"

typedef struct WorkQueueCell {
    _Atomic size_t sequence;
    TestCase*      data;
} WorkQueueCell;

typedef struct {
    size_t          capacity;
    size_t          mask;
    WorkQueueCell*  buffer;
    _Atomic size_t  enqueue_pos;
    _Atomic size_t  dequeue_pos;
} WorkQueue;

void      workqueue_init(WorkQueue* q, Corpus* corpus);
void      workqueue_destroy(WorkQueue* q);
void      workqueue_push(WorkQueue* q, TestCase* tc);
TestCase* workqueue_pop(WorkQueue* q);


#endif // WORKQUEUE_H
