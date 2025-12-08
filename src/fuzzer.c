// Source = https://clang.llvm.org/docs/SanitizerCoverage.html
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

#include "coverage.h"
#include "fuzzer.h"
#include "heap.h"
#include "interpreter_fuzz.h"
#include "type.h"
#include "value.h"
#include "vector.h"
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#include "ir_program.h"
#include "log.h"

#define SEEN_CAPACITY 32768
#define MUTATIONS_PER_PARENT  1000

static inline uint64_t now_us() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}

typedef struct {
  Vector* items;
  atomic_size_t next_index;
  pthread_mutex_t lock;
} WorkQueue;
static void workqueue_init(WorkQueue* q, Corpus* corpus)
{


  q->items = vector_new(sizeof(TestCase*));
  atomic_store(&q->next_index, (size_t)0);
  pthread_mutex_init(&q->lock, NULL);

  size_t n = corpus_size(corpus);
  for (size_t i = 0; i < n; i++) {
    TestCase* tc = corpus_get(corpus, i);
    if (tc)
      vector_push(q->items, &tc);
  }
}

static void workqueue_destroy(WorkQueue* q)
{
  vector_delete(q->items);
  pthread_mutex_destroy(&q->lock);
}

static void workqueue_push(WorkQueue* q, TestCase* tc)
{
  pthread_mutex_lock(&q->lock);
  vector_push(q->items, &tc);
  pthread_mutex_unlock(&q->lock);
}

static TestCase* workqueue_pop(WorkQueue* q)
{
  atomic_size_t* a = &q->next_index;

  size_t idx = atomic_fetch_add_explicit(
      a,
      (size_t)1,
      memory_order_relaxed
  );

  if (idx >= vector_length(q->items))
    return NULL;

  return *(TestCase**)vector_get(q->items, idx);
}

static size_t elem_size(const Type* t) {
    switch (t->kind) {
    case TK_INT:     return 1;
    case TK_CHAR:    return 1;
    case TK_BOOLEAN: return 1;
    default:
        return 1;
    }
}

static __thread uint32_t fuzz_rng_state = 0;
static __thread int fuzz_rng_inited = 0;

static uint32_t fuzz_rand_u32(void)
{
  if (!fuzz_rng_inited) {
    uint32_t t = (uint32_t)time(NULL);
    uint32_t tid = (uint32_t)(uintptr_t)pthread_self();

    fuzz_rng_state = t ^ (tid * 0x9e3779b9u);
    if (fuzz_rng_state == 0) fuzz_rng_state = 1;

    fuzz_rng_inited = 1;
  }

  fuzz_rng_state = fuzz_rng_state * 1664525u + 1013904223u;
  return fuzz_rng_state;
}

static inline size_t fuzz_rand_range(size_t max) {
  if (max == 0) return 0;
  return fuzz_rand_u32() % max;
}

static bool seen_insert(uint64_t *seen, int *seen_count, uint64_t h)
{
  for (int i = 0; i < *seen_count; i++)
    if (seen[i] == h)
      return true;

  if (*seen_count < SEEN_CAPACITY)
    seen[(*seen_count)++] = h;

  return false;
}

// TODO - Add abstract interpreter to mutation strategy
// Vector {[-10,10], [-INF, INF]}
// Vector {{INT_MIN, INT_MAX}}


// TODO - Implement concurrent manipulations


Fuzzer * fuzzer_init(size_t instruction_count, Vector* arg_types) {


  Fuzzer* f = malloc(sizeof(Fuzzer));
  f -> instruction_count = instruction_count;
  f -> cov_bytes = instruction_count;
  f -> corpus = corpus_init();
  f -> corpus_index = 0;

  size_t seed_len = 0;
    uint8_t* seed_data = fuzzer_make_seed(arg_types, &seed_len);
  uint8_t* empty_cov = calloc(f->cov_bytes, 1);

    TestCase* seed = create_testCase(seed_data, seed_len, empty_cov, f->cov_bytes);


  free(empty_cov);
  corpus_add(f->corpus, seed);
    return f;

}

Vector* fuzzer_run_until_complete(Fuzzer* f,
                                  const Method* method,
                                  const Config* config,
                                  const Options* opts,
                                  Vector* arg_types,
                                  int thread_count,
                                  Vector* interval_seeds) {

    Vector *all_interesting = vector_new(sizeof(TestCase *));

    for (size_t i = 0; i < vector_length(interval_seeds); i++) {
        TestCase *tc = *(TestCase**)vector_get(interval_seeds, i);

        corpus_add(f->corpus, tc);

        coverage_reset_thread_bitmap(tc->coverage_bitmap);

        Options base_opts = *opts;
        base_opts.parameters = NULL;

        VMContext *vm = fuzz_interpreter_setup(method, &base_opts, config, tc->coverage_bitmap);
        if (vm) {
            fuzz_VMContext_reset(vm);
            fuzz_interpreter_run(vm);
            coverage_commit_thread(tc->coverage_bitmap);
            fuzz_interpreter_free(vm);
        }

        vector_push(all_interesting, &tc);
    }

    printf("[FUZZER] Loaded %zu interval-guided seeds\n",
           vector_length(interval_seeds));

    if (vector_length(arg_types) == 0) {
        Vector *result = vector_new(sizeof(TestCase *));

        uint8_t *empty_cov = calloc(f->cov_bytes, 1);
        TestCase *tc = create_testCase(NULL, 0, empty_cov, f->cov_bytes);
        free(empty_cov);

        coverage_reset_thread_bitmap(tc->coverage_bitmap);

        Options base_opts = *opts;
        base_opts.parameters = NULL;

        VMContext *vm = fuzz_interpreter_setup(method, &base_opts, config, tc->coverage_bitmap);
        if (!vm) {
            return result;
        }

        fuzz_VMContext_reset(vm);
        RuntimeResult r = fuzz_interpreter_run(vm);

        coverage_commit_thread(tc->coverage_bitmap);

        fuzz_interpreter_free(vm);

        vector_push(result, &tc);

        return result;
    }

    uint64_t start = now_us();
    const uint64_t TIMEOUT_US = 10ULL * 1000000ULL;

    if (thread_count <= 1) {
        for (;;) {
            if (now_us() - start > TIMEOUT_US)
                return all_interesting;

            Vector *batch = fuzzer_run_single(f, method, config, opts, arg_types);

            size_t n = vector_length(batch);
            for (size_t i = 0; i < n; i++) {
                TestCase *tc = *(TestCase **) vector_get(batch, i);

                vector_push(all_interesting, &tc);
            }

            vector_delete(batch);

            if (coverage_is_complete()) {
                uint64_t end = now_us();
                printf("[FUZZER] Time taken: %.3f seconds\n",
                       (end - start) / 1e6);
                return all_interesting;
            }
        }
    }

    for (;;) {
        if (now_us() - start > TIMEOUT_US)
            return all_interesting;

        Vector *batch =
                fuzzer_run_parallel(f, method, config, opts, arg_types, thread_count);

        size_t n = vector_length(batch);
        for (size_t i = 0; i < n; i++) {
            TestCase *tc = *(TestCase **) vector_get(batch, i);

            vector_push(all_interesting, &tc);
        }

        vector_delete(batch);

        if (coverage_is_complete()) {
            uint64_t end = now_us();
            printf("[FUZZER] Time taken: %.3f seconds\n",
                   (end - start) / 1e6);
            return all_interesting;
        }
    }
}


Vector* fuzzer_run_single(Fuzzer* f,
                          const Method* method,
                          const Config* config,
                          const Options* opts,
                          Vector* arg_types)
{
    IrFunction* ir = ir_program_get_function_ir(method, config);
    if (!ir) {
        LOG_ERROR("Failed to build IR for method %s", method_get_id(method));
        return NULL;
    }

    WorkQueue queue;
    workqueue_init(&queue, f->corpus);

    Vector* interesting_global = vector_new(sizeof(TestCase*));

    uint64_t seen[SEEN_CAPACITY];
    int seen_count = 0;

    VMContext* vm = NULL;
    Heap* heap = NULL;

    for (;;) {
        TestCase* parent = workqueue_pop(&queue);
        if (!parent)
            break;

        for (int m = 0; m < MUTATIONS_PER_PARENT; m++) {

            TestCase* child = testCase_copy(parent);
            if (!child)
                continue;

            child = mutate(child, arg_types);
            if (!child)
                continue;

            uint64_t h = testcase_hash(child);
            bool already_seen = seen_insert(seen, &seen_count, h);

            uint8_t* thread_bitmap = child->coverage_bitmap;
            coverage_reset_thread_bitmap(thread_bitmap);

            if (!vm) {
                Options base_opts = *opts;
                base_opts.parameters = NULL;

                vm = fuzz_interpreter_setup(method, &base_opts, config, thread_bitmap);
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

            heap_reset(heap);

            int locals_count = 0;
            Value* new_locals = fuzz_build_locals_fast(heap, method,
                                                  child->data, child->len,
                                                  &locals_count);

            if (!new_locals) {
                testcase_free(child);
                continue;
            }

            fuzz_VMContext_set_locals(vm, new_locals, locals_count);
            fuzz_VMContext_reset(vm);

          RuntimeResult r = fuzz_interpreter_run(vm);


            size_t new_bits = coverage_commit_thread(thread_bitmap);
            bool crash = (r != RT_OK);

            if (new_bits > 0 && !already_seen) {
                vector_push(interesting_global, &child);
                corpus_add(f->corpus, child);
                workqueue_push(&queue, child);
                continue;
            }

            if (crash && !already_seen) {
                vector_push(interesting_global, &child);
                continue;
            }

            /* if (!already_seen) {
                corpus_add(f->corpus, child);
                workqueue_push(&queue, child);
                continue;
            } */

            testcase_free(child);
        }
    }

    if (vm)
        fuzz_interpreter_free(vm);

    workqueue_destroy(&queue);

    return interesting_global;
}


Vector* fuzzer_run_parallel(Fuzzer* f,
                            const Method* method,
                            const Config* config,
                            const Options* opts,
                            Vector* arg_types,
                            int thread_count)
{
    IrFunction* ir = ir_program_get_function_ir(method, config);
    if (!ir) {
        LOG_ERROR("Failed to build IR for method %s", method_get_id(method));
        return NULL;
    }

    WorkQueue queue;
    workqueue_init(&queue, f->corpus);

    Vector* global_interesting = vector_new(sizeof(TestCase*));
    pthread_mutex_t merge_lock = PTHREAD_MUTEX_INITIALIZER;

#pragma omp parallel num_threads(thread_count)
    {
        uint64_t seen[SEEN_CAPACITY];
        int seen_count = 0;

        Vector* local_interesting = vector_new(sizeof(TestCase*));

        VMContext* vm = NULL;
        Heap* heap = NULL;

        for (;;) {
            TestCase* parent = workqueue_pop(&queue);
            if (!parent) {
                break;
            }

            for (int m = 0; m < MUTATIONS_PER_PARENT; m++) {

                TestCase* child = testCase_copy(parent);
                if (!child) {
                    continue;
                }

                child = mutate(child, arg_types);
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

                    vm = fuzz_interpreter_setup(method, &base_opts, config, thread_bitmap);
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

                heap_reset(heap);

                int locals_count = 0;
                Value* new_locals = fuzz_build_locals_fast(heap, method,
                                                      child->data, child->len,
                                                      &locals_count);

                if (!new_locals) {
                    testcase_free(child);
                    continue;
                }

                fuzz_VMContext_set_locals(vm, new_locals, locals_count);
                fuzz_VMContext_reset(vm);


              RuntimeResult r = fuzz_interpreter_run(vm);

                size_t new_bits = coverage_commit_thread(thread_bitmap);

                bool crash = (r != RT_OK);

                if (new_bits > 0 && !already_seen) {
                    vector_push(local_interesting, &child);
                    corpus_add(f->corpus, child);
                    workqueue_push(&queue, child);
                    continue;
                }

                if (crash && !already_seen) {
                    vector_push(local_interesting, &child);
                    continue;
                }

               /*  if (!already_seen) {
                    corpus_add(f->corpus, child);
                    workqueue_push(&queue, child);
                    continue;
                } */

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

        if (vm) {
            fuzz_interpreter_free(vm);
        }

        vector_delete(local_interesting);
    }

    workqueue_destroy(&queue);

    return global_interesting;
}

TestCase* mutate(TestCase* tc, Vector* arg_types)
{
    if (!tc || tc->len == 0) return tc;
    if (!arg_types) return tc;

    size_t argc = vector_length(arg_types);
    if (argc == 0) return tc;

    uint8_t* buf = tc->data;
    size_t buf_len = tc->len;

    size_t arg_index = fuzz_rand_range(argc);
    Type* t = *(Type**)vector_get(arg_types, arg_index);

    size_t offset = 0;
    for (size_t i = 0; i < arg_index; i++) {
        Type* prev = *(Type**)vector_get(arg_types, i);
        switch (prev->kind) {
        case TK_INT:
        case TK_BOOLEAN:
        case TK_CHAR:
            if (offset + 1 > buf_len) return tc;
            offset += 1;
            break;

        case TK_ARRAY: {
            size_t esz = elem_size(prev->array.element_type);
            if (offset + 1 > buf_len) return tc;
            uint8_t len = buf[offset];
            size_t bytes = 1 + (size_t)len * esz;
            if (offset + bytes > buf_len) return tc;
            offset += bytes;
            break;
        }
        }
    }

    switch (t->kind) {

    case TK_INT: {
        if (offset + 1 > buf_len) return tc;
        uint8_t* p = buf + offset;
        switch (fuzz_rand_u32() % 3) {
            case 0: *p ^= (1u << (fuzz_rand_u32() % 8)); break;
            case 1: *p += (int8_t)((fuzz_rand_u32() % 5) - 2); break;
            case 2: *p = (uint8_t)fuzz_rand_u32(); break;
        }
        break;
    }

    case TK_BOOLEAN:
        if (offset + 1 > buf_len) return tc;
        buf[offset] ^= 1;
        break;

    case TK_CHAR: {
        if (offset + 1 > buf_len) return tc;
        uint8_t* p = buf + offset;
        uint8_t c = *p;
        switch (fuzz_rand_u32() % 5) {
            case 0: c += (int8_t)((fuzz_rand_u32() % 7) - 3); break;
            case 1: c ^= (uint8_t)(1u << (fuzz_rand_u32() % 8)); break;
            case 2: c = (uint8_t)(' ' + (fuzz_rand_u32() % 95)); break;
            case 3: c = (uint8_t)('a' + (fuzz_rand_u32() % 26)); break;
            case 4: c = (uint8_t)fuzz_rand_u32(); break;
        }
        *p = c;
        break;
    }

    case TK_ARRAY: {
        Type* elem = t->array.element_type;
        size_t esz = elem_size(elem);

        if (offset + 1 > buf_len) return tc;
        uint8_t len = buf[offset];
        size_t bytes = 1 + (size_t)len * esz;
        if (offset + bytes > buf_len) return tc;

        uint8_t* arr = buf + offset + 1;
        int action = fuzz_rand_u32() % 3;

        if (action == 0 && len < 255) {
            int grow_by = 1 + (fuzz_rand_u32() % 200);
            if (len + grow_by > 255) grow_by = 255 - len;

            size_t add_bytes = (size_t)grow_by * esz;
            size_t new_len_bytes = buf_len + add_bytes;
            uint8_t* new_buf = malloc(new_len_bytes);
            if (!new_buf) return tc;

            memcpy(new_buf, buf, offset);
            new_buf[offset] = len + grow_by;
            memcpy(new_buf + offset + 1, arr, (size_t)len * esz);

            uint8_t* dst = new_buf + offset + 1 + (size_t)len * esz;
            for (int k = 0; k < grow_by; k++) {
                switch (elem->kind) {
                case TK_INT:     *dst = 0; break;
                case TK_BOOLEAN: *dst = (uint8_t)(fuzz_rand_u32() & 1); break;
                case TK_CHAR: {
                    uint32_t r = fuzz_rand_u32() % 4;
                    uint8_t c;
                    if (r == 0) c = (uint8_t)('a' + (fuzz_rand_u32() % 26));
                    else if (r == 1) c = (uint8_t)(' ' + (fuzz_rand_u32() % 95));
                    else if (r == 2) c = (uint8_t)fuzz_rand_u32();
                    else c = 0;
                    *dst = c;
                    break;
                }
                default:
                    *dst = 0;
                }
                dst += esz;
            }

            size_t end = offset + 1 + (size_t)len * esz;
            memcpy(new_buf + offset + 1 + (size_t)(len + grow_by) * esz,
                   buf + end, buf_len - end);

            free(buf);
            tc->data = new_buf;
            tc->len = new_len_bytes;
            return tc;
        }

        /* shrink */
        if (action == 1 && len > 0) {
            size_t new_len_bytes = buf_len - esz;
            uint8_t* new_buf = malloc(new_len_bytes);
            if (!new_buf) return tc;

            memcpy(new_buf, buf, offset);
            new_buf[offset] = len - 1;
            memcpy(new_buf + offset + 1, arr, (size_t)(len - 1) * esz);

            size_t end = offset + 1 + (size_t)len * esz;
            memcpy(new_buf + offset + 1 + (size_t)(len - 1) * esz,
                   buf + end, buf_len - end);

            free(buf);
            tc->data = new_buf;
            tc->len = new_len_bytes;
            return tc;
        }

        if (len > 0) {
            size_t idx = fuzz_rand_range(len);
            uint8_t* ep = arr + idx * esz;

            switch (elem->kind) {
            case TK_INT:
                *ep += (int8_t)((fuzz_rand_u32() % 5) - 2);
                break;
            case TK_BOOLEAN:
                *ep ^= 1;
                break;
            case TK_CHAR: {
                uint8_t c = *ep;
                switch (fuzz_rand_u32() % 5) {
                    case 0: c += (int8_t)((fuzz_rand_u32() % 7) - 3); break;
                    case 1: c ^= (uint8_t)(1u << (fuzz_rand_u32() % 8)); break;
                    case 2: c = (uint8_t)(' ' + (fuzz_rand_u32() % 95)); break;
                    case 3: c = (uint8_t)('a' + (fuzz_rand_u32() % 26)); break;
                    default: c = (uint8_t)fuzz_rand_u32(); break;
                }
                *ep = c;
                break;
            }
            }
        }
        break;
    }

    default:
        break;
    }

    return tc;
}





uint8_t* fuzzer_make_seed(Vector* arg_types, size_t* out_len)
{
    size_t total = 0;

    // First compute the total buffer size.
    for (size_t i = 0; i < vector_length(arg_types); i++) {
        Type* t = *(Type**)vector_get(arg_types, i);

        switch (t->kind) {
        case TK_INT:
        case TK_BOOLEAN:
        case TK_CHAR:
            total += 1;
            break;

        case TK_ARRAY: {
            Type* elem = t->array.element_type;
            size_t esz = elem_size(elem);

            uint8_t len = 1;   // minimal seed length = 1
            total += 1;        // length byte
            total += len * esz;
            break;
        }
        }
    }

    uint8_t* buf = calloc(total, 1);
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    size_t offset = 0;

    for (size_t i = 0; i < vector_length(arg_types); i++) {
        Type* t = *(Type**)vector_get(arg_types, i);

        switch (t->kind) {

        case TK_INT:
            buf[offset++] = 0;     // 1-byte integer => 0
            break;

        case TK_BOOLEAN:
            buf[offset++] = 0;     // false
            break;

        case TK_CHAR:
            buf[offset++] = 'a';   // printable default
            break;

        case TK_ARRAY: {
            Type* elem = t->array.element_type;
            size_t esz = elem_size(elem);

            uint8_t len = 1;       // minimal seed array length
            buf[offset++] = len;

            for (uint8_t j = 0; j < len; j++) {
                switch (elem->kind) {
                case TK_INT:
                    buf[offset++] = 0;
                    break;

                case TK_BOOLEAN:
                    buf[offset++] = 0;
                    break;

                case TK_CHAR:
                    buf[offset++] = 'a';
                    break;

                default:
                    buf[offset++] = 0;
                    break;
                }
            }
            break;
        }

        }
    }

    *out_len = total;
    return buf;
}






void fuzzer_free(Fuzzer* f) {
  if (!f) return;

  corpus_free(f->corpus);

  free(f);
}
