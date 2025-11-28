#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

#include "cli.h"
#include "info.h"

#include <stdlib.h>

#define OPTIONS_NUM 2
#define MANDATORY_ARGS_NUM 1

#define INTERPRETER_TAG "i"
#define INTERPRETER_TAG_F "interpreter"
#define FUZZ_SHORT        "f"
#define FUZZ_LONG         "fuzzer"

static bool is_interpreter_only(const char* opt)
{
    return strcmp(opt, INTERPRETER_TAG) == 0 || strcmp(opt, INTERPRETER_TAG_F) == 0;
}

static bool is_fuzz_mode(const char* opt)
{
    return strcmp(opt, FUZZ_SHORT) == 0 ||
           strcmp(opt, FUZZ_LONG)  == 0;
}

OptionsParseResult options_parse_args(const int argc, const char** argv, Options* opts)
{
    int args_num = argc - 1;

    opts->info = false;
    opts->interpreter_only = false;
    opts->fuzzer = false;
    opts->method_id = NULL;
    opts->parameters = NULL;

    if (args_num < MANDATORY_ARGS_NUM) {
        return OPT_NOT_ENOUGH_ARGS;
    }

    if (args_num > OPTIONS_NUM + MANDATORY_ARGS_NUM) {
        return OPT_TOO_MANY_ARGS;
    }

    if (args_num > MANDATORY_ARGS_NUM) {
        for (int i = 1; i <= OPTIONS_NUM; i++) {
            if (argv[i][0] && argv[i][0] == '-') {
                if (is_interpreter_only(argv[i] + 1)) {
                    opts->interpreter_only = true;
                }
                else if (is_fuzz_mode(argv[i]+1)) {
                    opts->fuzzer = true;
                }
                // else if (other options) {...}
                else {
                    return OPT_OPTION_NOT_KNOWN;
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

    return OPT_OK;
}

void options_cleanup(Options* opts)
{
    if (opts) {
        free(opts->method_id);
        free(opts->parameters);
    }
}
