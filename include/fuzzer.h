#ifndef FUZZER_H
#define FUZZER_H

#include "vector.h"

void fuzzing_init();
char* fuzzer_random_parameters(Vector* arguments);

#endif
