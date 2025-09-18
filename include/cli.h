#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

typedef struct {
    bool info;
    bool interpreter_only;
    char* method_id;
} options;

int parse_args(const int argc, const char** argv, options* opts);
void delete_options(options* opts);

#endif
