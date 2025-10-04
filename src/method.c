#include "method.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define METHOD_CLASS_SEP '.'
#define METHOD_NAME_SEP ":"
#define METHOD_ARGUMENTS_FIRST "("
#define METHOD_ARGUMENTS_LAST ")"

#define SRC_PATH_MAX 256

#define DECOMPILED_FORMAT "json"
#define SOURCE_FORMAT "java"

typedef enum {
    MPR_OK,
    MPR_CLASS_MALFORMED,
    MPR_NAME_MALFORMED,
    MPR_RT_MALFORMED,
    MPR_INTERNAL_ERROR,
} MethodParseResult;

struct Method {
    char* class;
    char* name;
    char* arguments;
    char* return_type;
};

static const char* format[] = {
    [SRC_SOURCE] = SOURCE_FORMAT,
    [SRC_DECOMPILED] = DECOMPILED_FORMAT,
};

// all until the last .
static char* method_parse_class(char** method_id)
{
    if (!*method_id) {
        return NULL;
    }

    char* c = strrchr(*method_id, METHOD_CLASS_SEP);
    if (c == NULL) {
        return NULL;
    }

    *c = '\0';
    char* buffer = *method_id;

    *method_id = c + 1;
    return buffer;
}

// from the last . to :
static char* method_parse_name(char** method_id)
{
    if (!*method_id) {
        return NULL;
    }

    char* buffer = strsep(method_id, METHOD_NAME_SEP);
    return buffer;
}

// whats inside ()
static char* method_parse_arguments(char** method_id)
{
    strsep(method_id, METHOD_ARGUMENTS_FIRST);
    char* buffer = strsep(method_id, METHOD_ARGUMENTS_LAST);
    return buffer;
}

static char* method_parse_return_type(char* method_id)
{
    return method_id;
};

// in case of error code (>0), must call delete_method on m
static MethodParseResult method_parse(Method* m, char* method_id)
{
    char* buffer = method_parse_class(&method_id);
    if (!buffer) {
        return MPR_CLASS_MALFORMED;
    }
    m->class = strdup(buffer);

    buffer = method_parse_name(&method_id);
    if (!buffer) {
        return MPR_NAME_MALFORMED;
    }
    m->name = strdup(buffer);

    buffer = method_parse_arguments(&method_id);
    m->arguments = strdup(buffer);

    buffer = method_parse_return_type(method_id);
    if (!buffer) {
        return MPR_RT_MALFORMED;
    }
    m->return_type = strdup(buffer);

    return MPR_OK;
}

static MethodParseResult method_parse_from_invoke(
    Method* m,
    char* ref_name,
    char* method_name,
    char* args_type,
    char* return_type)
{
    char* method_id;
    replace_char(ref_name, '/', '.');

    // todo: parse the method directly, avoid overhead
    if (asprintf(&method_id, "%s.%s:(%s)%s", ref_name, method_name, args_type, return_type) < 0) {
        return MPR_INTERNAL_ERROR;
    }

    return method_parse(m, method_id);
}

Method* method_create(char* method_id)
{
    Method* m = calloc(1, sizeof(Method));
    if (m == NULL) {
        return NULL;
    }

    if (method_parse(m, method_id)) {
        method_delete(m);
        return NULL;
    }

    return m;
}

void method_delete(Method* m)
{
    if (m) {
        free(m->class);
        free(m->name);
        free(m->arguments);
        free(m->return_type);
    }

    free(m);
}

void method_print(const Method* m)
{
    printf("method class:          %s\n", m->class);
    printf("method name:           %s\n", m->name);
    printf("method arguments:      %s\n", m->arguments);
    printf("method return_type:    %s\n", m->return_type);
}

char* method_read(const Method* m, const Config* cfg, SourceType src)
{
    char path[SRC_PATH_MAX];
    char* source = NULL;

    char* class_path = strdup(m->class);
    if (!class_path) {
        goto cleanup;
    }

    replace_char(class_path, '.', '/');

    char* dir = src == SRC_DECOMPILED ? config_get_decompiled(cfg) : config_get_source(cfg);

    // todo check for error in sprintf
    sprintf(path, "%s/%s.%s", dir, class_path, format[src]);

    struct stat buf;
    if (stat(path, &buf) < 0) {
        goto cleanup;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        goto cleanup;
    }

    size_t nitems = buf.st_size;

    source = malloc(sizeof(char) * nitems + 1);
    if (!source) {
        goto cleanup;
    }

    if (fread(source, sizeof(char), nitems, f) < nitems) {
        free(source);
        source = NULL;

        goto cleanup;
    }

cleanup:
    if (f) {
        fclose(f);
    }
    free(class_path);

    return source;
}

char* method_get_name(const Method* m)
{
    return m->name;
}

// todo: get as array of ValueType?
char* method_get_arguments(const Method* m)
{
    return m->arguments;
}
