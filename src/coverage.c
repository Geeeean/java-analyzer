#include "coverage.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    atomic_uchar *bytes;   // bitmap (array of _Atomic unsigned char)
    size_t nBits;          // total bits == max_blocks
    size_t nBytes;         // (nBits + 7) / 8
    atomic_uint_fast8_t enabled;   // gate for cov_hit
} coverage_state;

static coverage_state coverage;

bool coverage_init(const size_t size) {
    if (size == 0) return false;

    memset(&coverage, 0, sizeof(coverage));
    coverage.nBits  = size;
    coverage.nBytes = (size + 7) / 8;

    // allocate atomic elements
    coverage.bytes = calloc(coverage.nBytes, sizeof *coverage.bytes);
    if (!coverage.bytes) return false;

    atomic_store_explicit(&coverage.enabled, true, memory_order_relaxed);


    return true;
}

void coverage_reset(void) {
    if (!coverage.bytes) return;
    for (size_t i = 0; i < coverage.nBytes; i++) {
        atomic_store_explicit(&coverage.bytes[i], 0u, memory_order_relaxed);
    }
}

void coverage_enable(bool on) {
    atomic_store_explicit(&coverage.enabled, 0u, memory_order_relaxed);
}

void coverage_remove(void) {
    if (coverage.bytes) {
        free(coverage.bytes);
        coverage.bytes = NULL;
    }
    coverage.nBits = 0;
    coverage.nBytes = 0;
    atomic_store_explicit(&coverage.enabled, 0u, memory_order_relaxed);
}

void coverage_mark(const uint32_t id) {
    if (id >= coverage.nBits) return;
    if (!atomic_load_explicit(&coverage.enabled, memory_order_relaxed)) return;

    const size_t byte = id >> 3;
    const unsigned char mask = (unsigned char)(1u << (id & 7u));
    (void)atomic_fetch_or_explicit(&coverage.bytes[byte], mask, memory_order_relaxed);
}

bool block_is_covered(const uint32_t id) {
    if (id >= coverage.nBits) return false;

    const size_t byte = id >> 3;
    const unsigned char mask = (unsigned char)(1u << (id & 7u));
    const unsigned char v =
        atomic_load_explicit(&coverage.bytes[byte], memory_order_relaxed);
    return (v & mask) != 0;
}

size_t coverage_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < coverage.nBytes; i++) {            // iterate bytes
        unsigned char v =
            atomic_load_explicit(&coverage.bytes[i], memory_order_relaxed);

        if (i + 1 == coverage.nBytes) {
            const size_t valid_bits = coverage.nBits - i * 8;
            if (valid_bits < 8) {
                const unsigned char tail_mask =
                    (unsigned char)((1u << valid_bits) - 1u);
                v &= tail_mask;
            }
        }

        // SWAR
        v = (unsigned char)(v - ((v >> 1) & 0x55));
        v = (unsigned char)((v & 0x33) + ((v >> 2) & 0x33));
        count += (size_t)((v + (v >> 4)) & 0x0F);             // swar algorithm https://www.playingwithpointers.com/blog/swar.html
    }
    return count;
}
