#include "utils.h"
#include "log.h"
#include "opcode.h"

#include "time.h"
#include <limits.h>
#include <string.h>

void replace_char(char* str, char find, char replace)
{
    char* pos = str;

    while ((pos = strchr(pos, find)) != NULL) {
        *pos = replace;
        pos++;
    }
}

double get_current_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
}

char* get_method_signature(InvokeOP* invoke)
{
    char* res = NULL;
    char* ref_name = strdup(invoke->ref_name);
    replace_char(ref_name, '/', '.');

    int capacity = 10;
    int args_len = 0;
    char* args = malloc(sizeof(char) * capacity);

    for (int i = 0; i < invoke->args_len; i++) {
        TypeKind tk = invoke->args[i]->kind;
        if (capacity <= args_len) {
            capacity *= 2;
            args = realloc(args, sizeof(char) * capacity);
            if (!args) {
                res = NULL;
                goto cleanup;
            }
        }

        switch (tk) {
        case TK_INT:
            args[args_len] = 'I';
            break;
        case TK_BOOLEAN:
            args[args_len] = 'Z';
            break;
        default:
            LOG_ERROR("Unable to handle type: %d, in get_method_signature", tk);
            return NULL;
        }

        args_len++;
    }

    args = realloc(args, sizeof(char) * args_len + 1);
    if (!args) {
        res = NULL;
        goto cleanup;
    }
    args[args_len] = '\0';

    char return_type[3];
    switch (invoke->return_type->kind) {
    case TK_INT:
        strcpy(return_type, "I");
        break;
    case TK_BOOLEAN:
        strcpy(return_type, "Z");
        break;
    case TK_VOID:
        strcpy(return_type, "V");
        break;
    case TK_ARRAY:
        if (invoke->return_type == make_array_type(TYPE_INT)) {
            strcpy(return_type, "[I");
        } else if (invoke->return_type == make_array_type(TYPE_BOOLEAN)) {
            strcpy(return_type, "[Z");
        } else {
            LOG_ERROR("Unable to handle array return type in get_method_signature");
            return NULL;
        }

        break;
    default:
        LOG_ERROR("Unable to handle return type: %d, in get_method_signature", invoke->return_type->kind);
        return NULL;
    }

    asprintf(&res, "%s.%s:(%s)%s", ref_name, invoke->method_name, args, return_type);

cleanup:
    free(args);
    free(ref_name);

    return res;
}
