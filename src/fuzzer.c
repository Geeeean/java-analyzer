// Source = https://clang.llvm.org/docs/SanitizerCoverage.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include "type.h"

// Generates a random parameter string, like "(5, true, 99)"
char* fuzzer_random_parameters(Vector* arguments)
{
    // arguments is a Vector<Type*>
    if (!arguments) return strdup("");

    size_t n = vector_length(arguments);

    char* buf = malloc(256);
    buf[0] = 0;

    strcat(buf, "(");
    for (size_t i = 0; i < n; i++) {
        Type* t = *(Type**)vector_get(arguments, i);

        if (type_is_array(t)) {
            // very simple array: [a,b,c]
            int len = rand() % 4;
            strcat(buf, "[");
            for (int j = 0; j < len; j++) {
                char tmp[32];
                sprintf(tmp, "%d", rand() % 20);
                strcat(buf, tmp);
                if (j + 1 < len) strcat(buf, ",");
            }
            strcat(buf, "]");
        }
        else if (t == TYPE_INT) {
            char tmp[32];
            sprintf(tmp, "%d", (rand() % 20) - 10);
            strcat(buf, tmp);
        }
        else if (t == TYPE_BOOLEAN) {
            strcat(buf, rand() % 2 ? "true" : "false");
        }
        else {
            strcat(buf, "0"); // fallback
        }

        if (i + 1 < n) strcat(buf, ",");
    }
    strcat(buf, ")");

    return buf;
}

