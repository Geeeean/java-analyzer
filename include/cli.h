#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

typedef struct {
    bool info;
    bool interpreter_only;
    char* method_id;
} Options;

int parse_args(const int argc, const char** argv, Options* opts);
void delete_options(Options* opts);

#endif
