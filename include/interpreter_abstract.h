#ifndef INTERPRETER_ABSTRACT_H
#define INTERPRETER_ABSTRACT_H

#include "cli.h"
#include "config.h"
#include "method.h"

typedef struct AbstractContext AbstractContext;

typedef struct {
    int num_locals;
    Vector** results; // Vector<Interval> results[num_locals];
} AbstractResult;

AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg);
AbstractResult interpreter_abstract_run(AbstractContext* abstract_context);
void abstract_result_print(const AbstractResult* result);
#endif
