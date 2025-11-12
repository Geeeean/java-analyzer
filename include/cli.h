#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

typedef struct {
    bool info;
    bool interpreter_only;
    bool abstract_only;
    char* method_id;
    char* parameters;
} Options;

typedef enum {
    OPT_OK,
    OPT_NOT_ENOUGH_ARGS,
    OPT_TOO_MANY_ARGS,
    OPT_OPTION_NOT_KNOWN
} OptionsParseResult;

OptionsParseResult options_parse_args(const int argc, const char** argv, Options* opts);
void options_cleanup(Options* opts);

#endif
