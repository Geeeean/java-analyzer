#ifndef COVERAGE_H
#define COVERAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


// Initializing the coverage map
bool coverage_init(size_t size);
void coverage_reset(void);
void coverage_enable(bool on);
void coverage_remove(void);

// Recording coverage

void coverage_mark(uint32_t id);
bool block_is_covered(uint32_t id);
size_t coverage_count(void);

#endif //COVERAGE_H