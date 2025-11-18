#ifndef COVERAGE_H
#define COVERAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


bool coverage_init(size_t nBits);

void coverage_mark(size_t pc);

void coverage_reset_current(void);

size_t coverage_commit(void);

size_t coverage_global_count(void);

size_t coverage_current_count(void);

void coverage_reset_all(void);

void coverage_global_print(size_t maxBits);
void coverage_current_print(size_t maxBits);


#endif //COVERAGE_H