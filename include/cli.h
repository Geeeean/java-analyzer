#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

typedef struct {
    bool info;
    bool interpreter_only;
    char* method_id;
    char* parameters;
} Options;

int options_parse_args(const int argc, const char** argv, Options* opts);
void options_cleanup(Options* opts);

#endif
