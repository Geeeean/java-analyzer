#ifndef METHOD_H
#define METHOD_H

#include "config.h"

typedef enum {
    SRC_SOURCE,
    SRC_DECOMPILED,
} SourceType;

typedef struct Method Method;

Method* method_create(char* method_id);
void method_delete(Method* m);
void method_print(const Method* m);
char* method_read(const Method* m, const Config* cfg, SourceType src);
char* method_get_name(const Method* m);

#endif
