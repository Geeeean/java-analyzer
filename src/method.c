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

static int sanity_check(const Method* m)
{
    if (m->class == NULL || !strlen(m->class)) {
        return 1;
    }

    if (m->name == NULL || !strlen(m->name)) {
        return 2;
    }

    if (m->arguments == NULL) {
        return 3;
    }

    // TODO
    // return type must be 1 char (or array [I ?)
    if (m->return_type == NULL || strlen(m->return_type) != 1) {
        return 4;
    }

    return 0;
}

// in case of error code (>0), must call delete_method on m
static int method_parse(Method* m, char* method_id)
{
    m->class = strdup(method_parse_class(&method_id));
    m->name = strdup(method_parse_name(&method_id));
    m->arguments = strdup(method_parse_arguments(&method_id));
    m->return_type = strdup(method_parse_return_type(method_id));

    return sanity_check(m);
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
    if (sanity_check(m)) {
        return;
    }

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
