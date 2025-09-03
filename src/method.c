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
    char* returns;
};

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

static char* get_method_name(char** method_id)
{
    char* buffer = strsep(method_id, METHOD_NAME_SEP);
    return buffer;
}

static char* get_method_arguments(char** method_id)
{
    strsep(method_id, METHOD_ARGUMENTS_FIRST);
    char* buffer = strsep(method_id, METHOD_ARGUMENTS_LAST);
    return buffer;
}

static void cleanup_method_partial(method* m)
{
    if (m->class)
        free(m->class);
    if (m->name)
        free(m->name);
    if (m->arguments)
        free(m->arguments);
    if (m->returns)
        free(m->returns);
}

//in case of error code (>0), must call delete_method on m
static int parse_method(method* m, char* method_id)
{
    m->class = strdup(get_method_class(&method_id));
    if (m->class == NULL) {
        return 1;
    }

    m->name = strdup(get_method_name(&method_id));
    if (m->name == NULL) {
        return 1;
    }

    m->arguments = strdup(get_method_arguments(&method_id));
    if (m->arguments == NULL) {
        return 1;
    }

    m->returns = strdup(method_id);
    if (m->returns == NULL) {
        return 1;
    }

    return 0;
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
    m->returns = NULL;

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
    printf("method class:      %s\n", m->class);
    printf("method name:       %s\n", m->name);
    printf("method arguments:  %s\n", m->arguments);
    printf("method returns:    %s\n", m->returns);
}
