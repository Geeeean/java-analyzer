#include "method.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METHOD_CLASS_SEP '.'
#define METHOD_NAME_SEP ":"
#define METHOD_ARGUMENTS_FIRST "("
#define METHOD_ARGUMENTS_LAST ")"

struct method {
    char* class;
    char* name;
    char* arguments;
    char* return_type;
};

// all until the last .
static char* get_method_class(char** method_id)
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
static char* get_method_name(char** method_id)
{
    char* buffer = strsep(method_id, METHOD_NAME_SEP);
    return buffer;
}

// whats inside ()
static char* get_method_arguments(char** method_id)
{
    strsep(method_id, METHOD_ARGUMENTS_FIRST);
    char* buffer = strsep(method_id, METHOD_ARGUMENTS_LAST);
    return buffer;
}

static char* get_method_return_type(char* method_id)
{
    if (method_id == NULL) {
        return NULL;
    }

    return method_id;
};

static void
cleanup_method_partial(method* m)
{
    if (m->class) {
        free(m->class);
    }

    if (m->name) {
        free(m->name);
    }

    if (m->arguments) {
        free(m->arguments);
    }

    if (m->return_type) {
        free(m->return_type);
    }
}

static int sanity_check(const method* m)
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
static int parse_method(method* m, char* method_id)
{
    m->class = strdup(get_method_class(&method_id));
    m->name = strdup(get_method_name(&method_id));
    m->arguments = strdup(get_method_arguments(&method_id));
    m->return_type = strdup(get_method_return_type(method_id));

    return sanity_check(m);
}

method* create_method(char* method_id)
{
    method* m = malloc(sizeof(method));
    if (m == NULL) {
        return NULL;
    }

    m->class = NULL;
    m->name = NULL;
    m->arguments = NULL;
    m->return_type = NULL;

    if (parse_method(m, method_id)) {
        delete_method(m);
        return NULL;
    }

    return m;
}

void delete_method(method* m)
{
    if (m == NULL) {
        return;
    }

    cleanup_method_partial(m);
    free(m);
}

void print_method(const method* m)
{
    if (sanity_check(m)) {
        return;
    }

    printf("method class:          %s\n", m->class);
    printf("method name:           %s\n", m->name);
    printf("method arguments:      %s\n", m->arguments);
    printf("method return_type:    %s\n", m->return_type);
}
