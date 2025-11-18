#include "coverage.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    uint8_t* global_bits;      // persistent fuzzy coverage
    uint8_t* current_bits;     // per-run coverage
    size_t nBits;
    bool is_initialized;
} coverage_state;

static coverage_state coverage = {0};



bool coverage_init(const size_t size) {
    if (size == 0) return false;

    if (coverage.is_initialized) {
        return false;
    }

    coverage.global_bits = calloc(size, sizeof(uint8_t));
    coverage.current_bits = calloc(size, sizeof(uint8_t));

    if (!coverage.global_bits || !coverage.current_bits) {
        free(coverage.global_bits);
        free(coverage.current_bits);
        coverage.global_bits = NULL;
        coverage.current_bits = NULL;
        return false;
    }

    coverage.nBits = size;
    coverage.is_initialized = true;


    return true;
}

void coverage_reset_current(void) {
    if (!coverage.is_initialized) return;
    memset(coverage.current_bits, 0, coverage.nBits);
}

void coverage_mark(size_t pc) {
    if (!coverage.is_initialized) return;
    if (pc >= coverage.nBits) return;

    coverage.current_bits[pc] = 1;
}

size_t coverage_commit(void) {
    if (!coverage.is_initialized) return 0;

    size_t new_bits = 0;
    for (size_t i = 0; i < coverage.nBits; i++) {
        if (coverage.current_bits[i] && !coverage.global_bits[i]) {
            coverage.global_bits[i] = 1;
            new_bits++;
        }
    }

    memset(coverage.current_bits, 0, coverage.nBits);
    return new_bits;
}

size_t coverage_current_count(void) {
    if (!coverage.is_initialized) return 0;

    size_t count = 0;
    for (size_t i = 0; i < coverage.nBits; i++) {
        count += (coverage.current_bits[i] != 0);
    }
    return count;
}

size_t coverage_global_count(void) {
    if (!coverage.is_initialized) return 0;

    size_t count = 0;
    for (size_t i = 0; i < coverage.nBits; i++) {
        count += (coverage.global_bits[i] != 0);
    }
    return count;
}

void coverage_reset_all(void) {
    if (!coverage.is_initialized) return;

    free(coverage.global_bits);
    free(coverage.current_bits);

    coverage.global_bits = NULL;
    coverage.current_bits = NULL;
    coverage.nBits = 0;
    coverage.is_initialized = false;
}

void coverage_global_print(size_t maxBits) {
    if (!coverage.is_initialized) return;

    size_t limit = maxBits < coverage.nBits ? maxBits : coverage.nBits;
    for (size_t i = 0; i < limit; i++) {
        putchar(coverage.global_bits[i] ? '1' : '0');
    }
    putchar('\n');
}

void coverage_current_print(size_t maxBits) {
    if (!coverage.is_initialized) return;

    size_t limit = maxBits < coverage.nBits ? maxBits : coverage.nBits;
    for (size_t i = 0; i < limit; i++) {
        putchar(coverage.current_bits[i] ? '1' : '0');
    }
    putchar('\n');
}

