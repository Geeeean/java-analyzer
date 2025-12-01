// Source = https://clang.llvm.org/docs/SanitizerCoverage.html
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "interpreter_abstract.h"
#include "interval_testcase.h"
#include "coverage.h"
#include "fuzzer.h"
#include "heap.h"
#include "interpreter_fuzz.h"
#include "log.h"
#include "type.h"
#include "value.h"
#include "vector.h"
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include <omp.h>
#include "workqueue.h"

#define SEEN_CAPACITY 32768
#define MUTATIONS_PER_PARENT 1000
static pthread_mutex_t coverage_lock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic uint64_t interp_time      = 0;
static _Atomic uint64_t mutate_time      = 0;
static _Atomic uint64_t build_locals_time = 0;
static _Atomic uint64_t heap_reset_time  = 0;
static _Atomic uint64_t queue_pop_time   = 0;
static _Atomic uint64_t commit_time = 0;
static _Atomic uint64_t lock_wait_time = 0;

static inline uint64_t now_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}

static __thread uint32_t fuzz_rng_state = 0;
static __thread int fuzz_rng_inited = 0;

static uint32_t fuzz_rand_u32(void)
{
    if (!fuzz_rng_inited) {
        uint32_t t = (uint32_t)time(NULL);
        uint32_t tid = (uint32_t)(uintptr_t)pthread_self();

        fuzz_rng_state = t ^ (tid * 0x9e3779b9u);
        if (fuzz_rng_state == 0)
            fuzz_rng_state = 1;

        fuzz_rng_inited = 1;
    }

    fuzz_rng_state = fuzz_rng_state * 1664525u + 1013904223u;
    return fuzz_rng_state;
}

static inline size_t fuzz_rand_range(size_t max)
{
    if (max == 0)
        return 0;
    return fuzz_rand_u32() % max;
}

static bool seen_insert(uint64_t* seen, int* seen_count, uint64_t h)
{
    for (int i = 0; i < *seen_count; i++)
        if (seen[i] == h)
            return true;

    if (*seen_count < SEEN_CAPACITY)
        seen[(*seen_count)++] = h;

    return false;
}

static size_t compute_required_len(Vector* arg_types)
{
    size_t total = 0;

    for (size_t i = 0; i < vector_length(arg_types); i++) {
        Type* t = *(Type**)vector_get(arg_types, i);
        if (!t)
            continue;

        switch (t->kind) {
            case TK_INT:
                total += 1;
                break;

            case TK_BOOLEAN:
            case TK_CHAR:
                total += 1;
                break;

            case TK_ARRAY:
                total += 1;
                break;

            default:
                total += 1;
                break;
        }
    }

    return total;
}

#define APPEND_FMT(buf, cursor, max, fmt, ...)                                    \
    do {                                                                          \
        int n = snprintf((buf) + *(cursor), (max) - *(cursor), fmt, __VA_ARGS__); \
        if (n < 0 || (size_t)n >= (max) - *(cursor))                              \
            return false;                                                         \
        *(cursor) += (size_t)n;                                                   \
    } while (0)

static bool append_escaped_char(char* buffer, size_t* cursor, size_t max, char c)
{
    if (c == '\'' || c == '\\' || c == '(' || c == ')' || c == ',' || c == ' ' || !isprint((unsigned char)c)) {
        APPEND_FMT(buffer, cursor, max, "'\\x%02x'", (unsigned char)c);
        return true;
    }

    APPEND_FMT(buffer, cursor, max, "'%c'", c);
    return true;
}

Fuzzer* fuzzer_init(size_t instruction_count, Vector* arg_types)
{
    Fuzzer* f = malloc(sizeof(Fuzzer));
    if (!f) {
        return NULL;
    }

    f->instruction_count = instruction_count;
    f->cov_bytes = instruction_count;
    f->corpus = corpus_init();
    f->corpus_index = 0;

    size_t seed_len = compute_required_len(arg_types);
    if (seed_len == 0) {
        seed_len = 1;
    }
    uint8_t* seed_data = calloc(seed_len, 1);
    if (!seed_data) {
        corpus_free(f->corpus);
        free(f);
        return NULL;
    }

    uint8_t* empty_cov = calloc(f->cov_bytes, 1);
    if (!empty_cov) {
        free(seed_data);
        corpus_free(f->corpus);
        free(f);
        return NULL;
    }
    TestCase* seed = create_testCase(seed_data, seed_len, empty_cov, f->cov_bytes);
    if (!seed) {
        free(seed_data);
        free(empty_cov);
        corpus_free(f->corpus);
        free(f);
        return NULL;
    }
    corpus_add(f->corpus, seed);

    return f;
}

Vector* fuzzer_run_until_complete(
        Fuzzer* f,
        const Method* method,
        const Config* config,
        const Options* opts,
        Vector* arg_types,
        int thread_count,
        AbstractResult* precomputed_abs)
{
    Vector *all_interesting = vector_new(sizeof(TestCase *));
    uint64_t start = now_us();
    uint64_t timeout_ms = 3000;

    bool has_array_arg = false;
    for (size_t i = 0; i < vector_length(arg_types); i++) {
        Type *t = *(Type **) vector_get(arg_types, i);
        if (t && t->kind == TK_ARRAY) {
            has_array_arg = true;
            break;
        }
    }

    abstract_result_print(precomputed_abs);

    if (!has_array_arg && precomputed_abs) {
        Vector *interval_seeds =
                generate_interval_seeds(f, precomputed_abs, arg_types);

        size_t s = vector_length(interval_seeds);
        for (size_t i = 0; i < s; i++) {
            TestCase *tc = *(TestCase **) vector_get(interval_seeds, i);
            vector_push(all_interesting, &tc);
        }

        vector_delete(interval_seeds);
    }

    print_corpus(f->corpus, arg_types);

    atomic_store(&interp_time,       0);
    atomic_store(&mutate_time,       0);
    atomic_store(&build_locals_time, 0);
    atomic_store(&heap_reset_time,   0);
    atomic_store(&queue_pop_time,    0);
    atomic_store(&lock_wait_time,    0);
    atomic_store(&commit_time,       0);

    WorkQueue wq;
    workqueue_init(&wq, f->corpus);

    size_t last_covered = (size_t)-1;
    size_t last_total   = (size_t)-1;
    for (;;) {
        uint64_t now_ms = (now_us() - start) / 1000;
        if (now_ms > timeout_ms) {
            LOG_INFO("Timeout reached — stopping fuzzing early");
            break;
        }

        if (coverage_is_complete()) {
            LOG_INFO("Coverage is complete — stopping fuzzing");
            break;
        }

        Vector *batch = NULL;
        if (thread_count <= 1) {
            batch = fuzzer_run_single(f, method, config, opts, arg_types, &wq);
        } else {
            batch = fuzzer_run_parallel(f, method, config, opts, arg_types, thread_count, &wq);
        }

        if (!batch) {
            LOG_INFO("No more work in queue — stopping fuzzing");
            break;
        }

        size_t n = vector_length(batch);
        if (n == 0) {
            vector_delete(batch);
            LOG_INFO("Empty batch returned — stopping fuzzing");
            break;
        }

        for (size_t i = 0; i < n; i++) {
            TestCase *tc = *(TestCase **) vector_get(batch, i);
            vector_push(all_interesting, &tc);
        }
        vector_delete(batch);

        size_t covered = 0, total = 0;
        coverage_get_stats(&covered, &total);

        if (covered != last_covered || total != last_total) {
            printf("Current coverage after batch: %zu / %zu\n", covered, total);
            last_covered = covered;
            last_total   = total;
        }
    }
    workqueue_destroy(&wq);
    printf("\n=== Coverage lock debug info ===\n");
    printf("Total time waiting for coverage lock: %lu us\n",
           atomic_load(&lock_wait_time));
    printf("Total time running commit function:   %lu us\n",
           atomic_load(&commit_time));
    printf("===================================\n");

    printf("\n=== Fuzzer hotspot breakdown (aggregated) ===\n");
    printf("Total time in mutate():          %lu us\n", atomic_load(&mutate_time));
    printf("Total time in heap_reset():      %lu us\n", atomic_load(&heap_reset_time));
    printf("Total time in build_locals_fast(): %lu us\n", atomic_load(&build_locals_time));
    printf("Total time in interpreter_run(): %lu us\n", atomic_load(&interp_time));
    printf("Total time in workqueue_pop():   %lu us\n", atomic_load(&queue_pop_time));
    printf("===========================================\n");

    printf("Time_spent: %lu microseconds\n", (now_us() - start));

    return all_interesting;
}



Vector* fuzzer_run_single(Fuzzer* f,
                          const Method* method,
                          const Config* config,
                          const Options* opts,
                          Vector* arg_types,
                          WorkQueue* workQueue)
{
    uint64_t start_us = now_us();

    uint64_t local_interp_time       = 0;
    uint64_t local_mutate_time       = 0;
    uint64_t local_build_locals_time = 0;
    uint64_t local_heap_reset_time   = 0;
    uint64_t local_queue_pop_time    = 0;
    uint64_t local_lock_wait_time    = 0;
    uint64_t local_commit_time       = 0;

    Vector* interesting_global = vector_new(sizeof(TestCase*));

    uint64_t seen[SEEN_CAPACITY];
    int seen_count = 0;

    VMContext* vm = NULL;
    Heap* heap = NULL;

    for (;;) {
        uint64_t q0 = now_us();
        TestCase* parent = workqueue_pop(workQueue);
        uint64_t q1 = now_us();
        local_queue_pop_time += (q1 - q0);

        if (!parent)
            break;

        for (int m = 0; m < MUTATIONS_PER_PARENT; m++) {

            uint64_t mu0 = now_us();
            TestCase* child = mutate(parent, arg_types);
            uint64_t mu1 = now_us();
            local_mutate_time += (mu1 - mu0);

            if (!child)
                continue;

            uint64_t h = testcase_hash(child);
            bool already_seen = seen_insert(seen, &seen_count, h);

            uint8_t* thread_bitmap = child->coverage_bitmap;
            coverage_reset_thread_bitmap(thread_bitmap);

            if (!vm) {
                Options base_opts = *opts;
                base_opts.parameters = NULL;

                vm = fuzz_interpreter_setup(method,
                                            &base_opts,
                                            config,
                                            thread_bitmap);
                if (!vm) {
                    testcase_free(child);
                    break;
                }

                heap = fuzz_VMContext_get_heap(vm);
                if (!heap) {
                    fuzz_interpreter_free(vm);
                    vm = NULL;
                    testcase_free(child);
                    break;
                }
            } else {
                fuzz_VMContext_set_coverage_bitmap(vm, thread_bitmap);
            }

            uint64_t hr0 = now_us();
            heap_reset(heap);
            uint64_t hr1 = now_us();
            local_heap_reset_time += (hr1 - hr0);

            int locals_count = 0;
            uint64_t bl0 = now_us();
            Value* new_locals = fuzz_build_locals_fast(heap, method,
                                                       child->data, child->len,
                                                       &locals_count);
            uint64_t bl1 = now_us();
            local_build_locals_time += (bl1 - bl0);

            if (!new_locals) {
                testcase_free(child);
                continue;
            }

            fuzz_VMContext_set_locals(vm, new_locals, locals_count);
            fuzz_VMContext_reset(vm);

            uint64_t ir0 = now_us();
            RuntimeResult r = fuzz_interpreter_run(vm);
            uint64_t ir1 = now_us();
            local_interp_time += (ir1 - ir0);

            // In single-threaded mode, no need for mutex
            uint64_t t1 = now_us();
            size_t new_bits = coverage_commit_thread(thread_bitmap);
            uint64_t t2 = now_us();

            local_lock_wait_time += 0;  // No lock wait in single-threaded mode
            local_commit_time    += (t2 - t1);

            bool crash = (r != RT_OK);

            if (new_bits > 0 && !already_seen) {
                vector_push(interesting_global, &child);
                corpus_add(f->corpus, child);
                workqueue_push(workQueue, child);
                continue;
            }

            if (crash && !already_seen) {
                vector_push(interesting_global, &child);
                corpus_add(f->corpus, child);
                continue;
            }

            if (!already_seen && (fuzz_rand_u32() % 500 == 0)) {
                corpus_add(f->corpus, child);
                workqueue_push(workQueue, child);
                continue;
            }

            testcase_free(child);
        }
    }

    if (vm)
        fuzz_interpreter_free(vm);

    uint64_t end_us = now_us();
    uint64_t total_us = end_us - start_us;
    (void)total_us; // currently unused, but kept if you want later

    atomic_fetch_add(&interp_time,       local_interp_time);
    atomic_fetch_add(&mutate_time,       local_mutate_time);
    atomic_fetch_add(&build_locals_time, local_build_locals_time);
    atomic_fetch_add(&heap_reset_time,   local_heap_reset_time);
    atomic_fetch_add(&queue_pop_time,    local_queue_pop_time);
    atomic_fetch_add(&lock_wait_time,    local_lock_wait_time);
    atomic_fetch_add(&commit_time,       local_commit_time);

    return interesting_global;
}




Vector* fuzzer_run_parallel(Fuzzer* f,
                            const Method* method,
                            const Config* config,
                            const Options* opts,
                            Vector* arg_types,
                            int thread_count,
                            WorkQueue* workQueue)
{
    uint64_t start_us = now_us();
    (void)start_us; // currently unused; keep if you want total time

    Vector* global_interesting = vector_new(sizeof(TestCase*));
    pthread_mutex_t merge_lock = PTHREAD_MUTEX_INITIALIZER;

#pragma omp parallel num_threads(thread_count)
    {
        uint64_t seen[SEEN_CAPACITY];
        int seen_count = 0;
        uint64_t local_interp_time       = 0;
        uint64_t local_mutate_time       = 0;
        uint64_t local_build_locals_time = 0;
        uint64_t local_heap_reset_time   = 0;
        uint64_t local_queue_pop_time    = 0;
        // Note: we could also track lock_wait/commit here if needed

        Vector* local_interesting = vector_new(sizeof(TestCase*));

        VMContext* vm = NULL;
        Heap* heap = NULL;

        for (;;) {
            uint64_t q0 = now_us();
            TestCase* parent = workqueue_pop(workQueue);
            uint64_t q1 = now_us();
            local_queue_pop_time += (q1 - q0);
            if (!parent) {
                break;
            }

            for (int m = 0; m < MUTATIONS_PER_PARENT; m++) {

                uint64_t mu0 = now_us();
                TestCase* child = mutate(parent, arg_types);
                uint64_t mu1 = now_us();
                local_mutate_time += (mu1 - mu0);

                if (!child) {
                    continue;
                }

                uint64_t h = testcase_hash(child);
                bool already_seen = seen_insert(seen, &seen_count, h);

                uint8_t* thread_bitmap = child->coverage_bitmap;
                coverage_reset_thread_bitmap(thread_bitmap);

                if (!vm) {
                    Options base_opts = *opts;
                    base_opts.parameters = NULL;

                    vm = fuzz_interpreter_setup(method,
                                                &base_opts,
                                                config,
                                                thread_bitmap);
                    if (!vm) {
                        testcase_free(child);
                        break;
                    }

                    heap = fuzz_VMContext_get_heap(vm);
                    if (!heap) {
                        fuzz_interpreter_free(vm);
                        vm = NULL;
                        testcase_free(child);
                        break;
                    }
                } else {
                    fuzz_VMContext_set_coverage_bitmap(vm, thread_bitmap);
                }

                uint64_t hr0 = now_us();
                heap_reset(heap);
                uint64_t hr1 = now_us();
                local_heap_reset_time += (hr1 - hr0);

                int locals_count = 0;
                uint64_t bl0 = now_us();
                Value* new_locals = fuzz_build_locals_fast(heap, method,
                                                           child->data, child->len,
                                                           &locals_count);
                uint64_t bl1 = now_us();
                local_build_locals_time += (bl1 - bl0);

                if (!new_locals) {
                    testcase_free(child);
                    continue;
                }

                fuzz_VMContext_set_locals(vm, new_locals, locals_count);
                fuzz_VMContext_reset(vm);

                uint64_t ir0 = now_us();
                RuntimeResult r = fuzz_interpreter_run(vm);
                uint64_t ir1 = now_us();
                local_interp_time += (ir1 - ir0);


                size_t new_bits = coverage_commit_thread(thread_bitmap);

                bool crash = (r != RT_OK);

                if (new_bits > 0 && !already_seen) {
                    vector_push(local_interesting, &child);
                    corpus_add(f->corpus, child);
                    workqueue_push(workQueue, child);
                    continue;
                }

                if (crash && !already_seen) {
                    vector_push(local_interesting, &child);
                    corpus_add(f->corpus, child);
                    continue;
                }

                if (!already_seen && (fuzz_rand_u32() % 500 == 0)) {
                    corpus_add(f->corpus, child);
                    workqueue_push(workQueue, child);
                    continue;
                }

                testcase_free(child);
            }

            if (!vm) {
                break;
            }
        }

        pthread_mutex_lock(&merge_lock);
        size_t n = vector_length(local_interesting);
        for (size_t i = 0; i < n; i++) {
            TestCase* tc = *(TestCase**)vector_get(local_interesting, i);
            vector_push(global_interesting, &tc);
        }
        pthread_mutex_unlock(&merge_lock);

        vector_delete(local_interesting);

        if (vm) {
            fuzz_interpreter_free(vm);
        }

        atomic_fetch_add(&interp_time,       local_interp_time);
        atomic_fetch_add(&mutate_time,       local_mutate_time);
        atomic_fetch_add(&build_locals_time, local_build_locals_time);
        atomic_fetch_add(&heap_reset_time,   local_heap_reset_time);
        atomic_fetch_add(&queue_pop_time,    local_queue_pop_time);
    }

    return global_interesting;


    printf("\n=== Coverage lock debug info ===\n");
    printf("Total time waiting for coverage lock: %lu us\n",
           atomic_load(&lock_wait_time));
    printf("Total time running commit function:   %lu us\n",
           atomic_load(&commit_time));
    printf("===================================\n");

    printf("\n=== Fuzzer hotspot breakdown (parallel) ===\n");
    printf("Total time in mutate():          %lu us\n", atomic_load(&mutate_time));
    printf("Total time in heap_reset():      %lu us\n", atomic_load(&heap_reset_time));
    printf("Total time in build_locals_fast(): %lu us\n", atomic_load(&build_locals_time));
    printf("Total time in interpreter_run(): %lu us\n", atomic_load(&interp_time));
    printf("Total time in workqueue_pop():   %lu us\n", atomic_load(&queue_pop_time));
    printf("===========================================\n");

    return global_interesting;
}

TestCase* mutate(TestCase* original, Vector* arg_types)
{
    if (!original || original->len == 0)
        return NULL;

    TestCase* tc = testCase_copy(original);
    if (!tc)
        return NULL;

    uint8_t* data = tc->data;
    size_t len = tc->len;

    size_t arg_count = vector_length(arg_types);
    if (arg_count == 0)
        return tc;

    size_t arg_index = fuzz_rand_range(arg_count);
    Type* t = *(Type**)vector_get(arg_types, arg_index);

    if (!t)
        return tc;

    // compute offset
    size_t offset = 0;
    for (size_t i = 0; i < arg_index; i++) {
        Type* prev = *(Type**)vector_get(arg_types, i);
        if (!prev) return tc;

        switch (prev->kind) {
            case TK_INT:
            case TK_BOOLEAN:
            case TK_CHAR:
                offset += 1;
                break;

            case TK_ARRAY: {
                if (offset >= len) return tc;
                uint8_t arr_len = data[offset];
                offset += 1 + arr_len;
                break;
            }

            default:
                return tc;
        }
    }

    if (offset >= len)
        return tc;

    if (t->kind == TK_INT) {
        int8_t v = (int8_t)data[offset];
        switch (fuzz_rand_u32() % 4) {
            case 0: v ^= (1u << (fuzz_rand_u32() % 7)); break;
            case 1: v += (int8_t)((fuzz_rand_u32() % 7) - 3); break;
            case 2: v = (int8_t)(fuzz_rand_u32() % 256); break;
            case 3: v = (int8_t)(-v); break;
        }
        data[offset] = (uint8_t)v;
        return tc;
    }

    if (t->kind == TK_BOOLEAN) {
        data[offset] ^= 1;
        return tc;
    }

    if (t->kind == TK_CHAR) {
        uint8_t c = data[offset];
        if (c < 0x20 || c > 0x7E) c = 'a';
        else c++;
        data[offset] = c;
        return tc;
    }

    if (t->kind == TK_ARRAY) {
        uint8_t arr_len = data[offset];

        if (offset + 1 + arr_len > len)
            return tc;

        int action = fuzz_rand_u32() % 3;

        if (action == 0 && arr_len < 50) {
            size_t new_len = len + 1;
            uint8_t* buf = malloc(new_len);
            if (!buf) return tc;

            memcpy(buf, data, offset + 1 + arr_len);

            buf[offset] = arr_len + 1;

            buf[offset + 1 + arr_len] = (uint8_t)(fuzz_rand_u32() % 256);

            memcpy(buf + offset + 1 + arr_len + 1,
                   data + offset + 1 + arr_len,
                   len - (offset + 1 + arr_len));

            free(tc->data);
            tc->data = buf;
            tc->len = new_len;
            return tc;
        }

        if (action == 1 && arr_len > 0) {
            size_t new_len = len - 1;
            uint8_t* buf = malloc(new_len);
            if (!buf) return tc;

            memcpy(buf, data, offset + 1);

            buf[offset] = (uint8_t)(arr_len - 1);

            memcpy(buf + offset + 1,
                   data + offset + 1,
                   arr_len - 1);

            memcpy(buf + offset + 1 + (arr_len - 1),
                   data + offset + 1 + arr_len,
                   len - (offset + 1 + arr_len));

            free(tc->data);
            tc->data = buf;
            tc->len = new_len;
            return tc;
        }

        if (arr_len > 0) {
            size_t idx = fuzz_rand_range(arr_len);
            data[offset + 1 + idx] ^= (1u << (fuzz_rand_u32() % 7));
        }

        return tc;
    }

    return tc;
}


void fuzzer_free(Fuzzer* f)
{
    if (!f)
        return;

    corpus_free(f->corpus);

    free(f);
}

void print_corpus(const Corpus* corpus, Vector* arg_types)
{
    printf("\n=== CORPUS CONTENTS (%zu testcases) ===\n",
           corpus_size(corpus));

    for (size_t i = 0; i < corpus_size(corpus); i++) {

        TestCase* tc = corpus_get((Corpus*) corpus, i);
        if (!tc) continue;

        printf("[%zu]  [", i);

        for (size_t b = 0; b < tc->len; b++) {
            printf("%02x", tc->data[b]);
            if (b + 1 < tc->len) printf(",");
        }

        printf("]\n");
    }

    printf("=======================================\n");
}
