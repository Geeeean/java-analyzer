#ifndef JAVA_ANALYZER_INTERVAL_TESTCASE_H
#define JAVA_ANALYZER_INTERVAL_TESTCASE_H
#pragma once
#include "vector.h"
#include "fuzzer.h"
#include "interpreter_abstract.h"
#include "testCaseCorpus.h"

Vector* generate_interval_seeds(Fuzzer* f,
                                AbstractResult* abs,
                                Vector* arg_types);

#endif //JAVA_ANALYZER_INTERVAL_TESTCASE_H