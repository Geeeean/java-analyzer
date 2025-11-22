#include "coverage.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    uint8_t* global_bits;
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

    if (!coverage.global_bits) {
        free(coverage.global_bits);
        coverage.global_bits = NULL;
        return false;
    }

    coverage.nBits = size;
    coverage.is_initialized = true;


    return true;
}

uint8_t* coverage_create_thread_bitmap() {
    if (!coverage.is_initialized) return NULL;
    uint8_t* bitmap = calloc(coverage.nBits, sizeof(uint8_t));
    return bitmap;
}

void coverage_reset_thread_bitmap(uint8_t* bitmap) {
    memset(bitmap, 0, coverage.nBits);
}

void coverage_mark_thread(uint8_t* bitmap, size_t pc) {
    if (pc < coverage.nBits) {
        bitmap[pc] = 1;
    }
}

size_t coverage_commit_thread(const uint8_t* bitmap) {
    if (!coverage.is_initialized) return 0;

    size_t new_bits = 0;
    for (size_t i = 0; i < coverage.nBits; i++) {
        if (bitmap[i] && !coverage.global_bits[i]) {
            coverage.global_bits[i] = 1;
            new_bits++;
        }
    }
    return new_bits;
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

    coverage.global_bits = NULL;
    coverage.nBits = 0;
    coverage.is_initialized = false;
}


int check_bits (const uint8_t* bitmap, size_t bitmap_size) {
    if (!coverage.global_bits || !bitmap || !bitmap_size) {
      return 0;
    }
    int new_bit_count = 0;

     for (size_t i = 0; i < bitmap_size; i++) {
      if (bitmap[i] == 1 && coverage.global_bits[i] != 1)  {
        new_bit_count++;
      }
    }
    return new_bit_count;
}

void coverage_global_print(size_t maxBits) {
    if (!coverage.is_initialized) return;

    const size_t limit = maxBits < coverage.nBits ? maxBits : coverage.nBits;
    for (size_t i = 0; i < limit; i++) {
        putchar(coverage.global_bits[i] ? '1' : '0');
    }
    putchar('\n');
}

