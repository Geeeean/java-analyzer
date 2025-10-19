#ifndef METHOD_H
#define METHOD_H

#include "config.h"
#include "vector.h"

typedef enum {
    SRC_SOURCE,
    SRC_DECOMPILED,
} SourceType;

typedef struct Method Method;

Method* method_create(char* method_id);
void method_delete(Method* m);
void method_print(const Method* m);
char* method_read(const Method* m, const Config* cfg, SourceType src);
char* method_get_class(const Method* m);
char* method_get_name(const Method* m);
char* method_get_arguments(const Method* m);
char* method_get_id(const Method* m);
Vector* method_get_arguments_as_types(const Method* m);

#endif
