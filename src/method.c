#include "method.h"
#include "log.h"
#include "type.h"
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
    MPR_ALLOC_ERROR,
    MPR_INTERNAL_ERROR,
} MethodParseResult;

struct Method {
    char* method_id;
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
    if (!(*method_id)) {
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
    if (!m->class) {
        return MPR_ALLOC_ERROR;
    }

    buffer = method_parse_name(&method_id);
    if (!buffer) {
        return MPR_NAME_MALFORMED;
    }

    m->name = strdup(buffer);
    if (!m->name) {
        return MPR_ALLOC_ERROR;
    }

    buffer = method_parse_arguments(&method_id);

    m->arguments = strdup(buffer);
    if (!m->arguments) {
        return MPR_ALLOC_ERROR;
    }

    buffer = method_parse_return_type(method_id);
    if (!buffer) {
        return MPR_RT_MALFORMED;
    }

    m->return_type = strdup(buffer);
    if (!m->return_type) {
        return MPR_ALLOC_ERROR;
    }

    return MPR_OK;
}

Method* method_create(char* method_id)
{
    Method* m = calloc(1, sizeof(Method));
    if (m == NULL) {
        return NULL;
    }

    m->method_id = strdup(method_id);

    int parse = method_parse(m, method_id);
    if (parse) {
        LOG_ERROR("While parsing %s: %d", method_id, parse);
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
        free(m->method_id);
    }

    free(m);
}

void method_print(const Method* m)
{
    LOG_INFO("method class:          %s", m->class);
    LOG_INFO("method name:           %s", m->name);
    LOG_INFO("method arguments:      %s", m->arguments);
    LOG_INFO("method return_type:    %s", m->return_type);
}

char* method_read(const Method* m, const Config* cfg, SourceType src)
{
    char path[SRC_PATH_MAX];
    char* source = NULL;
    FILE* f = NULL;

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
        LOG_ERROR("Unable to access %s or not found", path);
        goto cleanup;
    }

    f = fopen(path, "r");
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

    source[nitems] = '\0';

cleanup:
    if (f) {
        fclose(f);
    }
    free(class_path);

    return source;
}

char* method_get_class(const Method* m)
{
    return m->class;
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

Vector* method_get_arguments_as_types(const Method* m)
{
    if (!m->arguments) {
        return NULL;
    }

    Vector* v = vector_new(sizeof(Type*));

    char* arguments = strdup(m->arguments);
    char* arguments_consume = arguments;

    while (*arguments_consume != '\0') {
        Type* type = get_type(&arguments_consume);
        LOG_DEBUG("TYPE");

        if (!type || vector_push(v, &type)) {
            vector_delete(v);
            v = NULL;
        }

        arguments_consume++;
    }

cleanup:
    free(arguments);

    return v;
}

char* method_get_id(const Method* m)
{
    return m->method_id;
}

