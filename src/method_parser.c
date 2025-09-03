#include "method_parser.h"
#include <stdio.h>
#include <string.h>

#define METHOD_NAME_SEP ":"
#define METHOD_PARAMS_FIRST "("
#define METHOD_PARAMS_LAST ")"

// return a ptr to the class name,
// return NULL if the method_id is not well formed
char* get_method_name(char** method_id)
{
    char* buffer = strsep(method_id, METHOD_NAME_SEP);
    return buffer;
}

char* get_method_params(char **method_id) {
    strsep(method_id, METHOD_PARAMS_FIRST);
    char* buffer = strsep(method_id, METHOD_PARAMS_LAST);
    return buffer;
}
