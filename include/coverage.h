#ifndef COVERAGE_H
#define COVERAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


bool coverage_init(size_t nBits);

uint8_t* coverage_create_thread_bitmap();

void coverage_reset_thread_bitmap(uint8_t* bitmap);

void coverage_mark_thread(uint8_t* bitmap, size_t pc);

size_t coverage_commit_thread(const uint8_t* bitmap);

size_t coverage_global_count(void);


void coverage_reset_all(void);

int check_bits(const uint8_t* bitmap, size_t size);

void coverage_global_print(size_t maxBits);

bool coverage_is_complete(void);


#endif //COVERAGE_H