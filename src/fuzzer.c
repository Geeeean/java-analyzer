#include "fuzzer.h"
#include "log.h"
#include "time.h"
#include "type.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool get_random_bool()
{
    return rand() % 2;
}

static int get_random_int()
{
    return (random() - (RAND_MAX / 2)) % 10;
}

void fuzzing_init()
{
    LOG_DEBUG("FUZZING");
    srandom(time(NULL));
}

char* fuzzer_random_parameters(Vector* arguments)
{
    if (!arguments) {
        LOG_ERROR("Arguments vector is NULL");
        return NULL;
    }

    size_t capacity = 512;
    size_t len = 0;
    char* buf = malloc(capacity);
    if (!buf) {
        return NULL;
    }
    buf[0] = '\0';

    LOG_DEBUG("ARG LEN: %d", vector_length(arguments));

    for (size_t i = 0; i < vector_length(arguments); ++i) {
        Type* type = *(Type**)vector_get(arguments, i);
        char tmp[64];
        int n = 0;

        if (type == TYPE_INT) {
            n = snprintf(tmp, sizeof(tmp), "%d,", get_random_int());
        } else if (type == TYPE_BOOLEAN) {
            n = snprintf(tmp, sizeof(tmp), "%s,", get_random_bool() ? "true" : "false");
        } else if (type == TYPE_CHAR) {
            int c = get_random_int() & 0xFF;
            n = snprintf(tmp, sizeof(tmp), "'%c',", isprint(c) ? c : '?');
        } else if (type == make_array_type(TYPE_INT)) {
            strcpy(tmp, "[I:1,2,3,4,5]");
        } else if (type == make_array_type(TYPE_BOOLEAN)) {
            strcpy(tmp, "[Z: true, false]");
        } else if (type == make_array_type(TYPE_CHAR)) {
            strcpy(tmp, "[C:'a','b','c']");
        } else {
            LOG_ERROR("Unknown type in fuzzer random pars");
            return NULL;
        }

        LOG_DEBUG("%d", n);

        if (len + n + 2 > capacity) {
            capacity = (len + n + 2) * 2;
            char* tmpbuf = realloc(buf, capacity);
            if (!tmpbuf) {
                free(buf);
                return NULL;
            }
            buf = tmpbuf;
        }

        memcpy(buf + len, tmp, n);
        len += n;
        buf[len] = '\0';
    }

    return buf;
}
