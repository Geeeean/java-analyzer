// Source = https://clang.llvm.org/docs/SanitizerCoverage.html
#include "fuzzer.h"
#include "coverage.h"
#include "heap.h"
#include "interpreter.h"
#include "log.h"
#include "type.h"
#include "vector.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

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


Fuzzer * fuzzer_init(size_t instruction_count) {
  Fuzzer* f = malloc(sizeof(Fuzzer));
  f -> instruction_count = instruction_count;
  f -> cov_bytes = instruction_count;
  f -> corpus = corpus_init();
  f -> corpus_index = 0;

  uint8_t empty[1] = {0};
  uint8_t* empty_cov = calloc(f->cov_bytes, 1);

  TestCase* seed = create_testCase(empty, 1, empty_cov, f->cov_bytes);

  free(empty_cov);
  corpus_add(f->corpus, seed);

  return f;

}

Vector* fuzzer_run(Fuzzer* f,
                   const Method* method,
                   const Config* config,
                   const Options* options,
                   Vector* arg_types) {
  int stagnant_iterations = 0;
  const size_t STAGNATION_LIMIT = 1000000;

  while (stagnant_iterations < STAGNATION_LIMIT) {

    TestCase* parent = corpus_choose(f->corpus, &f->corpus_index);
    TestCase* child  = testCase_copy(parent);
    child = mutate(child);

    Options local_opts = *options;
    local_opts.parameters = NULL;

    if (!parse(child->data, child->len, arg_types, &local_opts) ||
        !local_opts.parameters || local_opts.parameters[0] == '\0') {
      free(local_opts.parameters);
      local_opts.parameters = NULL;
      testcase_free(child);
      continue;
    }

    uint8_t* thread_bitmap = child->coverage_bitmap;
    coverage_reset_thread_bitmap(thread_bitmap);

    VMContext* vm = interpreter_setup(method, &local_opts, config, thread_bitmap);
    if (!vm) {
      free(local_opts.parameters);
      testcase_free(child);
      continue;
    }

    RuntimeResult r = interpreter_run(vm);
    (void)r;

    interpreter_free(vm);
    heap_reset();

    size_t new_bits = coverage_commit_thread(thread_bitmap);

    if (new_bits > 0) {
      corpus_add(f->corpus, child);
      stagnant_iterations = 0;
    } else if ((rand() % 500) == 0) {
      corpus_add(f->corpus, child);
    } else {
      testcase_free(child);
      stagnant_iterations++;
    }

    free(local_opts.parameters);
  }

  return f->corpus;
}


bool parse (const uint8_t* data,const size_t length,
            Vector* arg_types,
            Options* out_opts) {

  if (!data || !length || !arg_types || !out_opts) {
    return false;
  }



  const size_t arguments_len = vector_length(arg_types);


  char buffer[512]; // Holder for the bytestring parsed values

  size_t data_cursor = 0; // Cursor for choosing byte from bytestring
  size_t buffer_cursor = 0; // Curser for adding values into buffer

  // APPEND_FMT(buffer, &buffer_cursor, sizeof(buffer),"%c", '(');

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



TestCase* mutate(TestCase* tc) {
  if (!tc) {
    return NULL;
  }

  if (tc->len < 1) {
    return NULL;
  }
  size_t position = rand() % tc->len;

  tc -> fuzz_count++;

  switch (rand() % 5) {
  case 0: // bit-flip
    tc -> data[position] ^=  1U << (rand() % 8);
    break;
  case 1:
    tc -> data[position] = (uint8_t)rand();
    break;

  case 2:
    tc -> data[position] += (rand()&1) ? 1 : -1;
    break;

  case 3: {
    size_t larger_len = tc -> len + 1;
    uint8_t* larger_data = malloc(larger_len);

    if (!larger_data) {
      return tc;
    }

    memcpy(larger_data, tc->data, position);

    larger_data[position] = (uint8_t)rand();

    memcpy(larger_data + position + 1, tc->data + position, tc->len - position);

    free(tc->data);
    tc->data = larger_data;
    tc -> len = larger_len;
    break;
  }
  case 4: {
    if (tc -> len <= 1) {break;}

    size_t smaller_len = tc -> len - 1;
    uint8_t* smaller_data = malloc(smaller_len);
    if (!smaller_data) {
      return tc;
    }
    memcpy(smaller_data, tc->data, position);

    memcpy(smaller_data + position, tc->data + position + 1, tc->len - position - 1);

    free(tc->data);
    tc->data = smaller_data;
    tc -> len = smaller_len;
    break;
  }
  }

  return tc;
}

void fuzzer_free(Fuzzer* f) {
  if (!f) return;

  corpus_free(f->corpus);

  free(f);
}