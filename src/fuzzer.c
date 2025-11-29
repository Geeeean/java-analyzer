// Source = https://clang.llvm.org/docs/SanitizerCoverage.html
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

#include "coverage.h"
#include "fuzzer.h"
#include "heap.h"
#include "interpreter.h"
#include "log.h"
#include "type.h"
#include "value.h"
#include "vector.h"
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#define SEEN_CAPACITY 32768
#define MUTATIONS_PER_PARENT  1000
static pthread_mutex_t coverage_lock = PTHREAD_MUTEX_INITIALIZER;

// Returns microseconds
static inline uint64_t now_us() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}

typedef struct {
    Vector* items;
    size_t next_index;
    pthread_mutex_t lock;
} WorkQueue;

static void workqueue_init(WorkQueue* q, Vector* corpus)
{
    q->items = vector_new(sizeof(TestCase*));
    q->next_index = 0;
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
    pthread_mutex_lock(&q->lock);

    size_t idx = q->next_index;
    size_t len = vector_length(q->items);

    if (idx >= len) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    TestCase* tc = *(TestCase**)vector_get(q->items, idx);
    q->next_index++;

    pthread_mutex_unlock(&q->lock);
    return tc;
}


static __thread uint32_t fuzz_rng_state = 0;
static __thread int fuzz_rng_inited = 0;

static uint32_t fuzz_rand_u32(void)
{
  if (!fuzz_rng_inited) {
    uint32_t t = (uint32_t)time(NULL);
    uint32_t tid = (uint32_t)(uintptr_t)pthread_self();

    fuzz_rng_state = t ^ (tid * 0x9e3779b9u);  // simple mixing
    if (fuzz_rng_state == 0) fuzz_rng_state = 1;  // avoid zero state

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
      return true;     // already seen

  if (*seen_count < SEEN_CAPACITY)
    seen[(*seen_count)++] = h;

  return false;
}


static size_t compute_required_len(Vector* arg_types)
{
    size_t total = 0;
    for (size_t i = 0; i < vector_length(arg_types); i++) {
        Type* t = *(Type**) vector_get(arg_types, i);
        if (!t) continue; // or bail hard if you prefer

        switch (t->kind) {
        case TK_INT:
        case TK_BOOLEAN:
        case TK_CHAR:
            total += 1;      // 1 byte each
            break;

        case TK_ARRAY:
            total += 1;      // length byte, initially 0 → no payload
            break;

        default:
            // unknown → still give it 1 byte so we don't create empty testcases
            total += 1;
            break;
        }
    }
    return total;
}



#define APPEND_FMT(buf, cursor, max, fmt, ...) do { \
int n = snprintf((buf) + *(cursor), (max) - *(cursor), fmt, __VA_ARGS__); \
if (n < 0 || (size_t)n >= (max) - *(cursor)) return false; \
*(cursor) += (size_t)n; \
} while (0)  // macro for safe-guarding buffer overflow


static bool append_escaped_char(char *buffer, size_t *cursor, size_t max, char c) {
  // Characters that break tokenizer OR require escaping
  if (c == '\'' || c == '\\' || c == '(' || c == ')' || c == ',' || c == ' ' || !isprint((unsigned char)c)) {
    APPEND_FMT(buffer, cursor, max, "'\\x%02x'", (unsigned char)c);
    return true;
  }

  // All other printable characters are safe
  APPEND_FMT(buffer, cursor, max, "'%c'", c);
  return true;
}


// TODO - Add abstract interpreter to mutation strategy
// Vector {[-10,10], [-INF, INF]}
// Vector {{INT_MIN, INT_MAX}}


// TODO - Implement concurrent manipulations


Fuzzer* fuzzer_init(size_t instruction_count, Vector* arg_types)
{
    Fuzzer* f = malloc(sizeof(Fuzzer));
    if (!f) return NULL;

    f->instruction_count = instruction_count;
    f->cov_bytes         = instruction_count;
    f->corpus            = corpus_init();
    f->corpus_index      = 0;

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



Vector* fuzzer_run_until_complete(Fuzzer* f,
                                  const Method* method,
                                  const Config* config,
                                  const Options* opts,
                                  Vector* arg_types,
                                  int thread_count)
{
  Vector* all_interesting = vector_new(sizeof(TestCase*));
    uint64_t start = now_us();
    uint64_t timeout_ms = 3000;

  if (thread_count <= 1) {

    for (;;) {
        if ((now_us() - start) / 1000 > timeout_ms) {
            LOG_INFO("Timeout reached — stopping fuzzing early");
        }
      Vector* batch =
          fuzzer_run_single(f, method, config, opts, arg_types);

      size_t n = vector_length(batch);
      for (size_t i = 0; i < n; i++) {
        TestCase* tc = *(TestCase**) vector_get(batch, i);
        vector_push(all_interesting, &tc);
      }

      vector_delete(batch);

      if (coverage_is_complete())
        return all_interesting;
    }
  }

  for (;;) {
      if ((now_us() - start) / 1000 > timeout_ms) {
          LOG_INFO("Timeout reached — stopping fuzzing early");
          break;
      }
    Vector* batch =
        fuzzer_run_parallel(f, method, config, opts, arg_types, thread_count);

    size_t n = vector_length(batch);
    for (size_t i = 0; i < n; i++) {
      TestCase* tc = *(TestCase**) vector_get(batch, i);
      vector_push(all_interesting, &tc);
    }

    vector_delete(batch);

      pthread_mutex_lock(&coverage_lock);
      bool done = coverage_is_complete();
      pthread_mutex_unlock(&coverage_lock);

      if (done)
          return all_interesting;
  }
}

Vector* fuzzer_run_single(Fuzzer* f,
                          const Method* method,
                          const Config* config,
                          const Options* opts,
                          Vector* arg_types)
{
    // Build initial work queue from the corpus
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

            // ⬇⬇⬇ FIX: let mutate() do the copying
            TestCase* child = mutate(parent, arg_types);
            if (!child)
                continue;
            // ⬆⬆⬆ no extra testCase_copy here anymore

            uint64_t h = testcase_hash(child);
            bool already_seen = seen_insert(seen, &seen_count, h);

            uint8_t* thread_bitmap = child->coverage_bitmap;
            coverage_reset_thread_bitmap(thread_bitmap);

            // Lazily create IR-based persistent VM
            if (!vm) {
                Options base_opts = *opts;
                base_opts.parameters = NULL;

                vm = persistent_ir_interpreter_setup(method,
                                                     &base_opts,
                                                     config,
                                                     thread_bitmap);
                if (!vm) {
                    testcase_free(child);
                    break;
                }

                heap = VMContext_get_heap(vm);
                if (!heap) {
                    interpreter_free(vm);
                    vm = NULL;
                    testcase_free(child);
                    break;
                }
            } else {
                VMContext_set_coverage_bitmap(vm, thread_bitmap);
            }

            // Reuse heap for next child
            heap_reset(heap);

            int locals_count = 0;
            Value* new_locals = build_locals_fast(heap, method,
                                                  child->data, child->len,
                                                  &locals_count);

            if (!new_locals) {
                testcase_free(child);
                continue;
            }

            VMContext_set_locals(vm, new_locals, locals_count);
            VMContext_reset(vm);

            RuntimeResult r = interpreter_run(vm);

            size_t new_bits;
            pthread_mutex_lock(&coverage_lock);
            new_bits = coverage_commit_thread(thread_bitmap);
            pthread_mutex_unlock(&coverage_lock);

            bool crash = (r != RT_OK);

            if (new_bits > 0 && !already_seen) {
                vector_push(interesting_global, &child);
                corpus_add(f->corpus, child);
                workqueue_push(&queue, child);
                continue;
            }

            if (crash && !already_seen) {
                vector_push(interesting_global, &child);
                corpus_add(f->corpus, child);
                continue;
            }

            if (!already_seen && (fuzz_rand_u32() % 500 == 0)) {
                corpus_add(f->corpus, child);
                workqueue_push(&queue, child);
                continue;
            }

            testcase_free(child);
        }
    }

    if (vm)
        interpreter_free(vm);

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

                TestCase* child = mutate(parent, arg_types);
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

                    vm = persistent_ir_interpreter_setup(method,
                                                         &base_opts,
                                                         config,
                                                         thread_bitmap);
                    if (!vm) {
                        testcase_free(child);
                        break;
                    }

                    heap = VMContext_get_heap(vm);
                    if (!heap) {
                        interpreter_free(vm);
                        vm = NULL;
                        testcase_free(child);
                        break;
                    }
                } else {
                    VMContext_set_coverage_bitmap(vm, thread_bitmap);
                }

                heap_reset(heap);

                int locals_count = 0;
                Value* new_locals = build_locals_fast(heap, method,
                                                      child->data, child->len,
                                                      &locals_count);

                if (!new_locals) {
                    testcase_free(child);
                    continue;
                }

                VMContext_set_locals(vm, new_locals, locals_count);
                VMContext_reset(vm);

                RuntimeResult r = interpreter_run(vm);

                size_t new_bits;
                pthread_mutex_lock(&coverage_lock);
                new_bits = coverage_commit_thread(thread_bitmap);
                pthread_mutex_unlock(&coverage_lock);

                bool crash = (r != RT_OK);

                if (new_bits > 0 && !already_seen) {
                    vector_push(local_interesting, &child);
                    corpus_add(f->corpus, child);
                    workqueue_push(&queue, child);
                    continue;
                }

                if (crash && !already_seen) {
                    vector_push(local_interesting, &child);
                    corpus_add(f->corpus, child);
                    continue;
                }

                if (!already_seen && (fuzz_rand_u32() % 500 == 0)) {
                    corpus_add(f->corpus, child);
                    workqueue_push(&queue, child);
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
            interpreter_free(vm);
        }
    }

    workqueue_destroy(&queue);

    return global_interesting;
}





Vector* fuzzer_run_thread(Fuzzer* f,
                          const Method* method,
                          const Config* config,
                          const Options* options,
                          Vector* arg_types)
{
    int stagnant_iterations = 0;
    const size_t STAGNATION_LIMIT = 10000;

    uint64_t seen[SEEN_CAPACITY];
    int seen_count = 0;

    Vector* interesting = vector_new(sizeof(TestCase*));

    VMContext* persistent_vm = NULL;
    Heap* heap = NULL;

    // each thread starts at a different offset (you already have thread-local RNG)
    size_t local_index = fuzz_rand_u32();

    while (stagnant_iterations < STAGNATION_LIMIT) {

        size_t corpus_count = corpus_size(f->corpus);

        if (corpus_count == 0) {
            stagnant_iterations++;
            continue;
        }

        local_index %= corpus_count;
        TestCase* parent = corpus_get(f->corpus, local_index);
        local_index++;

        if (!parent) {
            stagnant_iterations++;
            continue;
        }

        TestCase* child = testCase_copy(parent);
        if (!child) {
            stagnant_iterations++;
            continue;
        }

        child = mutate(child, arg_types);
        if (!child) {
            stagnant_iterations++;
            continue;
        }

        uint64_t h = testcase_hash(child);
        bool already_seen = seen_insert(seen, &seen_count, h);

        uint8_t* thread_bitmap = child->coverage_bitmap;
        coverage_reset_thread_bitmap(thread_bitmap);

        // Lazily create IR-based persistent VM once
        if (!persistent_vm) {
            Options local_opts = *options;
            local_opts.parameters = NULL;

            persistent_vm =
                persistent_ir_interpreter_setup(method, &local_opts, config, thread_bitmap);

            if (!persistent_vm) {
                testcase_free(child);
                stagnant_iterations++;
                continue;
            }

            heap = VMContext_get_heap(persistent_vm);
            if (!heap) {
                testcase_free(child);
                interpreter_free(persistent_vm);
                persistent_vm = NULL;
                break;
            }
        } else {
            // Reuse VM, only update coverage bitmap
            VMContext_set_coverage_bitmap(persistent_vm, thread_bitmap);
        }

        heap_reset(heap);

        int locals_count = 0;
        Value* new_locals =
            build_locals_fast(heap, method, child->data, child->len, &locals_count);

        if (!new_locals) {
            testcase_free(child);
            stagnant_iterations++;
            continue;
        }

        VMContext_set_locals(persistent_vm, new_locals, locals_count);
        VMContext_reset(persistent_vm);

        RuntimeResult r = interpreter_run(persistent_vm);

        size_t new_bits;
        pthread_mutex_lock(&coverage_lock);
        new_bits = coverage_commit_thread(thread_bitmap);
        pthread_mutex_unlock(&coverage_lock);

        bool crash = (r != RT_OK);

        if (new_bits > 0 && !already_seen) {
            vector_push(interesting, &child);

            corpus_add(f->corpus, child);

            stagnant_iterations = 0;
            continue;
        }

        if (crash && !already_seen) {
            vector_push(interesting, &child);
            corpus_add(f->corpus, child);
            continue;
        }

        if (!already_seen && (fuzz_rand_u32() % 500 == 0)) {
            corpus_add(f->corpus, child);
            continue;
        }

        testcase_free(child);
        stagnant_iterations++;
    }

    if (persistent_vm)
        interpreter_free(persistent_vm);

    return interesting;
}

bool parse (const uint8_t* data,const size_t length,
            Vector* arg_types,
            Options* out_opts) {

  if (!data || !length || !arg_types || !out_opts) {
    return false;
  }



  const size_t arguments_len = vector_length(arg_types);


  char buffer[512];

  size_t data_cursor = 0;
  size_t buffer_cursor = 0;

  for (int i = 0; i < arguments_len; i++) {
    Type* t = *(Type**) vector_get(arg_types, i);
    if (!t) return false;
    if (data_cursor >= length) return false;

    switch (t -> kind) {
    case TK_INT: {
      int int_val = (int8_t) data[data_cursor++];
      APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%d", int_val);
      break;
    }
    case TK_BOOLEAN: {
      bool bool_val = (bool) data[data_cursor++];
      APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%s", bool_val ? "true" : "false");
      break;
    }
    case TK_CHAR: {
      char char_val = (char) data[data_cursor++];
      append_escaped_char(buffer, &buffer_cursor, sizeof(buffer), char_val);
      break;
    }
    case TK_ARRAY: {
      uint8_t raw_len = (uint8_t)data[data_cursor++];
      uint8_t array_length = raw_len;

      if (data_cursor + array_length > length) {
        return false;
      }

      APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c", '[');

      Type* arr_type = t->array.element_type;


      char type_char;
        switch (arr_type -> kind) {
        case TK_INT:
          type_char = 'I';
          break;
        case TK_CHAR:
          type_char = 'C';
          break;
        case TK_BOOLEAN:
          type_char = 'Z';
          break;
        default:
          return false;
        }

        // Write element type
        APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c", type_char);

        // EMPTY ARRAY → close immediately
        if (array_length == 0) {
          APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c", ']');
          break;
        }

        // Non-empty → write colon
        APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c", ':');

      for (int j = 0; j < array_length; j++) {
        switch (arr_type -> kind) {
        case TK_INT:
          int iv = (int8_t) data[data_cursor++];
          APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%d", iv);
          break;
        case TK_CHAR:
          char cv = (char) data[data_cursor++];
          append_escaped_char(buffer, &buffer_cursor, sizeof(buffer), cv);
          break;
        case TK_BOOLEAN:
          bool bv = (bool) data[data_cursor++];
          APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%s", bv ? "true" : "false");
          break;
        default:
          return false;
        }


        if (j + 1 < array_length) {
          APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c",',');
        }
      }


      APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c", ']');
      break;
    }


      default:
      return false;



    }

    if (i + 1 < arguments_len) { // add comma for each new entry
      APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer), "%c", ',');
    }
  }

  if (buffer_cursor >= sizeof(buffer) - 1) return false;
 // buffer[buffer_cursor++] = ')';
  buffer[buffer_cursor]   = '\0'; // append )\0


  free(out_opts->parameters);
  out_opts->parameters = strdup(buffer);
  if (!out_opts->parameters) {
    // Out of memory – treat as parse failure
    return false;
  }

  return true;
}

TestCase* mutate(TestCase* original, Vector* arg_types)
{
    if (!original || original->len == 0) return NULL;

    // Always clone the original; mutations NEVER touch input testcase in-place
    TestCase* tc = testCase_copy(original);
    if (!tc) return NULL;

    uint8_t* data = tc->data;
    size_t len = tc->len;

    size_t arg_count = vector_length(arg_types);
    if (arg_count == 0) return tc;

    // Choose argument to mutate
    size_t arg_index = fuzz_rand_range(arg_count);
    Type* t = *(Type**)vector_get(arg_types, arg_index);
    if (!t) return tc;

    // --- Compute offset ---
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

    if (offset >= len) return tc;

    // ---- Perform mutation (pure-copy semantics) ----
    switch (t->kind) {

    case TK_INT: {
        uint8_t v = data[offset];
        switch (fuzz_rand_u32() % 3) {
        case 0: v ^= (1u << (fuzz_rand_u32() % 8)); break;
        case 1: v = (uint8_t)(v + ((int8_t)((fuzz_rand_u32() % 5) - 2))); break;
        case 2: v = (uint8_t)fuzz_rand_u32(); break;
        }
        data[offset] = v;
        break;
    }

    case TK_BOOLEAN: {
        data[offset] ^= 1u;
        break;
    }

    case TK_CHAR: {
        uint8_t c = data[offset];
        if (c < 0x20 || c > 0x7E) c = 'a';
        else c++;
        data[offset] = c;
        break;
    }

    case TK_ARRAY: {
        uint8_t arr_len = data[offset];
        if (offset + 1 + arr_len > len) return tc;

        Type* elem_t = t->array.element_type;
        int action = fuzz_rand_u32() % 3;

        // ---- GROW ----
        if (action == 0 && arr_len < 50) {
            size_t new_len = len + 1;
            uint8_t* buf = malloc(new_len);
            if (!buf) return tc;

            memcpy(buf, data, offset + 1 + arr_len);

            buf[offset] = arr_len + 1;
            buf[offset + 1 + arr_len] =
                'a' + (fuzz_rand_u32() % 26); // Could type-check elem

            memcpy(buf + offset + 1 + arr_len + 1,
                   data + offset + 1 + arr_len,
                   len - (offset + 1 + arr_len));

            free(tc->data);
            tc->data = buf;
            tc->len = new_len;
            return tc;
        }

        // ---- SHRINK ----
        if (action == 1 && arr_len > 0) {
            size_t new_len = len - 1;
            uint8_t* buf = malloc(new_len);
            if (!buf) return tc;

            memcpy(buf, data, offset + 1); // includes len byte
            buf[offset] = arr_len - 1;

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

        // ---- MUTATE AN ELEMENT ----
        if (arr_len > 0) {
            size_t i = fuzz_rand_range(arr_len);
            uint8_t* elem = &data[offset + 1 + i];

            switch (elem_t->kind) {
            case TK_INT:
            case TK_CHAR:
                *elem = 'a' + (fuzz_rand_u32() % 26);
                break;
            case TK_BOOLEAN:
                *elem ^= 1u;
                break;
            default:
                break;
            }
        }
        break;
    }

    default:
        break;
    }

    return tc;
}




void fuzzer_free(Fuzzer* f) {
  if (!f) return;

  corpus_free(f->corpus);

  free(f);
}
