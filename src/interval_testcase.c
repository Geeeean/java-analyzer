#include "domain_interval.h"
#include "interval_testcase.h"
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int is_top_interval(Interval* iv) {
    return iv->lower == INT_MIN && iv->upper == INT_MAX;
}

static int is_bottom_interval(Interval* iv) {
    return iv->lower > iv->upper;
}

static int is_singleton(Interval* iv) {
    return iv->lower == iv->upper;
}

static Vector* interval_to_values(Interval* iv) {
    Vector* reps = vector_new(sizeof(int));
    if (is_bottom_interval(iv))
        return reps;
    if (is_singleton(iv)) {
        int v = iv->lower;
        vector_push(reps, &v);
        return reps;
    }
    if (is_top_interval(iv))
        return reps;

    int L = iv->lower;
    int U = iv->upper;
    vector_push(reps, &L);

    if (L < 0 && U > 0) {
        int mid = 0;
        vector_push(reps, &mid);
    } else {
        long long m = ((long long)L + (long long)U) / 2;
        int mid = (int)m;
        vector_push(reps, &mid);
    }

    vector_push(reps, &U);
    return reps;
}

static TestCase* build_testcase_from_values(Fuzzer* f,
                                            Vector* arg_types,
                                            Vector* arg_values_tuple)
{
    size_t arg_count = vector_length(arg_types);
    size_t len = 0;

    for (size_t i = 0; i < arg_count; i++) {
        Type* t = *(Type**) vector_get(arg_types, i);
        if (!t) return NULL;

        switch (t->kind) {
            case TK_INT:
                len += 1;
                break;
            case TK_BOOLEAN:
            case TK_CHAR:
                len += 1;
                break;
            case TK_ARRAY:
                return NULL;
            default:
                return NULL;
        }
    }

    uint8_t* buf = calloc(len, 1);
    if (!buf) return NULL;

    size_t cursor = 0;

    for (size_t i = 0; i < arg_count; i++) {
        int v = *(int*)vector_get(arg_values_tuple, i);
        Type* t = *(Type**) vector_get(arg_types, i);
        if (!t) {
            free(buf);
            return NULL;
        }

        switch (t->kind) {
            case TK_INT: {
                if (v < -128) v = -128;
                if (v > 127) v = 127;
                int8_t iv = (int8_t)v;
                buf[cursor++] = (uint8_t)iv;
                break;
            }
            case TK_BOOLEAN: {
                buf[cursor++] = (v != 0) ? 1u : 0u;
                break;
            }
            case TK_CHAR: {
                buf[cursor++] = (uint8_t)(unsigned char)v;
                break;
            }
            default:
                buf[cursor++] = 0;
                break;
        }
    }

    uint8_t* cov = calloc(f->cov_bytes, 1);
    if (!cov) {
        free(buf);
        return NULL;
    }

    TestCase* tc = create_testCase(buf, len, cov, f->cov_bytes);
    if (!tc) {
        free(buf);
        free(cov);
        return NULL;
    }

    return tc;
}

Vector* generate_interval_seeds(Fuzzer* f,
                                AbstractResult* abs,
                                Vector* arg_types)
{
    size_t arg_count = vector_length(arg_types);
    Vector* all_seeds = vector_new(sizeof(TestCase*));

    if (arg_count == 0) return all_seeds;

    Vector** arg_representatives = calloc(arg_count, sizeof(Vector*));

    for (size_t arg = 0; arg < arg_count; arg++) {
        Vector* reps = vector_new(sizeof(int));
        arg_representatives[arg] = reps;

        Vector* intervals = abs->results[arg];
        size_t n = vector_length(intervals);

        for (size_t i = 0; i < n; i++) {
            Interval* iv = (Interval*) vector_get(intervals, i);
            if (is_top_interval(iv)) continue;
            if (is_bottom_interval(iv)) continue;

            Vector* vals = interval_to_values(iv);
            for (size_t k = 0; k < vector_length(vals); k++) {
                int v = *(int*)vector_get(vals, k);
                vector_push(reps, &v);
            }
            vector_delete(vals);
        }

        if (vector_length(reps) == 0) {
            int zero = 0;
            vector_push(reps, &zero);
        }
    }

    size_t* idx = calloc(arg_count, sizeof(size_t));

    while (1) {
        Vector* tuple = vector_new(sizeof(int));
        for (size_t a = 0; a < arg_count; a++) {
            int v = *(int*)vector_get(arg_representatives[a], idx[a]);
            vector_push(tuple, &v);
        }

        TestCase* tc = build_testcase_from_values(f, arg_types, tuple);
        if (tc) {
            corpus_add(f->corpus, tc);
            vector_push(all_seeds, &tc);
        }

        vector_delete(tuple);

        size_t pos = arg_count - 1;
        while (1) {
            idx[pos]++;
            if (idx[pos] < vector_length(arg_representatives[pos]))
                break;
            idx[pos] = 0;
            if (pos == 0) goto done;
            pos--;
        }
    }

    free(idx);

    for (size_t a = 0; a < arg_count; a++) {
        vector_delete(arg_representatives[a]);
    }

    free(arg_representatives);
    return all_seeds;
}
