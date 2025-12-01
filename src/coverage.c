#include "coverage.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <time.h>

typedef struct {
    _Atomic uint8_t* global_bits;  // Make the bitmap atomic for lock-free access
    size_t nBits;
    bool is_initialized;
    _Atomic bool is_complete;
    _Atomic uint64_t last_new_coverage_time_us;
} coverage_state;

static coverage_state coverage = {0};

static size_t g_total_instructions = 0;      // you probably already have this
static uint8_t* g_global_bitmap = NULL;      // same here

void coverage_get_stats(size_t* covered, size_t* total)
{
    if (!covered || !total) return;

    *total = g_total_instructions;

    size_t cnt = 0;
    for (size_t i = 0; i < g_total_instructions; i++) {
        if (g_global_bitmap[i]) {
            cnt++;
        }
    }
    *covered = cnt;
}

// Consider fuzzing complete if no new coverage for this many microseconds (1 second)
#define STALE_COVERAGE_TIMEOUT_US 1000000

// Helper function to get current time in microseconds
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}



bool coverage_init(const size_t size) {
    if (size == 0) return false;

    if (coverage.is_initialized) {
        return false;
    }

    coverage.global_bits = calloc(size, sizeof(_Atomic uint8_t));

    if (!coverage.global_bits) {
        free(coverage.global_bits);
        coverage.global_bits = NULL;
        return false;
    }

    coverage.nBits = size;
    coverage.is_initialized = true;
    atomic_store(&coverage.is_complete, false);  // Initially not complete
    atomic_store(&coverage.last_new_coverage_time_us, get_time_us());

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
        if (bitmap[i]) {
            // Use atomic compare-and-swap to avoid race conditions
            uint8_t expected = 0;
            if (atomic_compare_exchange_strong(&coverage.global_bits[i], &expected, 1)) {
                // We successfully set a bit that was previously 0
                new_bits++;
            }
        }
    }
    
    // Update staleness timer and check for completion
    if (new_bits > 0) {
        // Found new coverage, update the timestamp
        atomic_store(&coverage.last_new_coverage_time_us, get_time_us());
    } else {
        // No new coverage, check if we've been stale too long
        uint64_t last_time = atomic_load(&coverage.last_new_coverage_time_us);
        uint64_t current_time = get_time_us();

        if (current_time - last_time >= STALE_COVERAGE_TIMEOUT_US) {
            atomic_store(&coverage.is_complete, true);
        }
    }

    // Also check if all bits are covered (ideal case)
    if (!atomic_load(&coverage.is_complete) && new_bits > 0) {
        bool all_covered = true;
        for (size_t i = 0; i < coverage.nBits; i++) {
            if (atomic_load(&coverage.global_bits[i]) == 0) {
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
        count += (atomic_load(&coverage.global_bits[i]) != 0);
    }
    return count;
}

void coverage_reset_all(void) {
    if (!coverage.is_initialized) return;

    free(coverage.global_bits);

    coverage.global_bits = NULL;
    coverage.nBits = 0;
    coverage.is_initialized = false;
    atomic_store(&coverage.is_complete, false);
    atomic_store(&coverage.last_new_coverage_time_us, 0);
}


int check_bits (const uint8_t* bitmap, size_t bitmap_size) {
    if (!coverage.global_bits || !bitmap || !bitmap_size) {
      return 0;
    }
    int new_bit_count = 0;

     for (size_t i = 0; i < bitmap_size; i++) {
      if (bitmap[i] == 1 && atomic_load(&coverage.global_bits[i]) != 1)  {
        new_bit_count++;
      }
    }
    return new_bit_count;
}

void coverage_global_print(size_t maxBits) {
    if (!coverage.is_initialized) return;

    const size_t limit = maxBits < coverage.nBits ? maxBits : coverage.nBits;
    for (size_t i = 0; i < limit; i++) {
        putchar(atomic_load(&coverage.global_bits[i]) ? '1' : '0');
    }
    putchar('\n');
}

bool coverage_is_complete(void) {
    if (!coverage.is_initialized) return false;
    return atomic_load(&coverage.is_complete);
}
