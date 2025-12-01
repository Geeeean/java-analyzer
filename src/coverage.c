#include "coverage.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <time.h>

typedef struct {
    uint8_t* global_bits;
    size_t   nBits;
    bool     is_initialized;
    _Atomic bool     is_complete;
    _Atomic uint64_t last_new_coverage_time_us;
} coverage_state;

static coverage_state coverage = {0};

// Consider fuzzing complete if no new coverage for this many microseconds (1 second)
#define STALE_COVERAGE_TIMEOUT_US 1000000

// Helper function to get current time in microseconds
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * Return current coverage stats:
 *  - *covered = number of bits set in global coverage bitmap
 *  - *total   = total number of instrumented instructions (bitmap size)
 */
void coverage_get_stats(size_t* covered, size_t* total)
{
    if (!covered || !total) {
        return;
    }

    if (!coverage.is_initialized) {
        *covered = 0;
        *total   = 0;
        return;
    }

    *total = coverage.nBits;

    size_t cnt = 0;
    for (size_t i = 0; i < coverage.nBits; i++) {
        if (coverage.global_bits[i] != 0) {
            cnt++;
        }
    }
    *covered = cnt;
}

bool coverage_init(const size_t size) {
    if (size == 0) return false;

    if (coverage.is_initialized) {
        // Already initialized; you might decide to allow re-init with same size
        return false;
    }

    coverage.global_bits = calloc(size, sizeof(uint8_t));
    if (!coverage.global_bits) {
        // calloc failed
        coverage.global_bits = NULL;
        return false;
    }

    coverage.nBits = size;
    coverage.is_initialized = true;
    atomic_store(&coverage.is_complete, false);  // Initially not complete
    atomic_store(&coverage.last_new_coverage_time_us, get_time_us());

    return true;
}

uint8_t* coverage_create_thread_bitmap(void) {
    if (!coverage.is_initialized) return NULL;
    uint8_t* bitmap = calloc(coverage.nBits, sizeof(uint8_t));
    return bitmap;
}

void coverage_reset_thread_bitmap(uint8_t* bitmap) {
    if (!coverage.is_initialized || !bitmap) return;
    memset(bitmap, 0, coverage.nBits);
}

void coverage_mark_thread(uint8_t* bitmap, size_t pc) {
    if (!coverage.is_initialized || !bitmap) return;
    if (pc < coverage.nBits) {
        bitmap[pc] = 1;
    }
}

size_t coverage_commit_thread(const uint8_t* bitmap) {
    if (!coverage.is_initialized || !bitmap) return 0;

    size_t new_bits = 0;
    for (size_t i = 0; i < coverage.nBits; i++) {
        if (bitmap[i] && !coverage.global_bits[i]) {
            coverage.global_bits[i] = 1;
            new_bits++;
        }
    }

    if (new_bits > 0) {
        atomic_store(&coverage.last_new_coverage_time_us, get_time_us());
    }

    if (new_bits > 0) {
        bool all_covered = true;
        for (size_t i = 0; i < coverage.nBits; i++) {
            if (coverage.global_bits[i] == 0) {
                all_covered = false;
                break;
            }
        }
        if (all_covered) {
            atomic_store(&coverage.is_complete, true);
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
    coverage.nBits       = 0;
    coverage.is_initialized = false;
    atomic_store(&coverage.is_complete, false);
    atomic_store(&coverage.last_new_coverage_time_us, 0);
}

int check_bits (const uint8_t* bitmap, size_t bitmap_size) {
    if (!coverage.global_bits || !bitmap || bitmap_size == 0) {
        return 0;
    }

    int new_bit_count = 0;
    size_t limit = (bitmap_size < coverage.nBits) ? bitmap_size : coverage.nBits;

    for (size_t i = 0; i < limit; i++) {
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

bool coverage_is_complete(void) {
    if (!coverage.is_initialized) return false;
    return atomic_load(&coverage.is_complete);
}
