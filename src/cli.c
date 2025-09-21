#include "cli.h"
#include "info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPTIONS_NUM 2
#define MANDATORY_ARGS_NUM 1

#define INTERPRETER_TAG "i"
#define INTERPRETER_TAG_F "interpreter"

static bool is_interpreter_only(const char* opt)
{
    return strcmp(opt, INTERPRETER_TAG) == 0 || strcmp(opt, INTERPRETER_TAG_F) == 0;
}

int options_parse_args(const int argc, const char** argv, Options* opts)
{
    int args_num = argc - 1;

    opts->info = false;
    opts->interpreter_only = false;
    opts->method_id = NULL;
    opts->parameters = NULL;

    if (args_num < MANDATORY_ARGS_NUM || args_num > OPTIONS_NUM + MANDATORY_ARGS_NUM) {
        return 1;
    }

    if (args_num > MANDATORY_ARGS_NUM) {
        for (int i = 1; i <= OPTIONS_NUM; i++) {
            if (argv[i][0] && argv[i][0] == '-') {
                if (is_interpreter_only(argv[i] + 1)) {
                    opts->interpreter_only = true;
                }
                // else if (other options) {...}
                else {
                    return 2;
                }
            }
        }
    }

    const char* method_id = NULL;
    const char* parameters = NULL;

    if (opts->interpreter_only) {
        method_id = argv[args_num - 1];
        parameters = argv[args_num];
    } else {
        method_id = argv[args_num];
    }

    if (is_info(method_id)) {
        opts->info = true;
    } else {
        opts->method_id = strdup(method_id);
    }

    if (opts->interpreter_only) {
        opts->parameters = strdup(parameters);
    }

    return 0;
}

void options_cleanup(Options* opts)
{
    if (opts) {
        free(opts->method_id);
        free(opts->parameters);
    }
}
