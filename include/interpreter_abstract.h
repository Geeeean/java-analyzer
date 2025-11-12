#ifndef INTERPRETER_ABSTRACT_H
#define INTERPRETER_ABSTRACT_H

#include "cli.h"
#include "config.h"
#include "method.h"

typedef struct AbstractContext AbstractContext;

AbstractContext* interpreter_abstract_setup(const Method* m, const Options* opts, const Config* cfg);
void interpreter_abstract_run(AbstractContext* abstract_context);
// void interpreter_abstract_run(VMContext* vm_context);

#endif
