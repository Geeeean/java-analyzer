#include "cli.h"
#include "config.h"
#include "info.h"
#include "method.h"
#include "syntax.h"

#include "tree_sitter/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    options opts;
    if (parse_args(argc, (const char**)argv, &opts) > 0) {
        fprintf(stderr, "Correct usage: ./analyzer <options> <methodid>\n");
        return 1;
    }

    config* cfg = load_config();
    if (cfg == NULL) {
        fprintf(stderr, "Config file is wrongly formatted or not exist\n");
        return 2;
    }

    if (opts.info) {
        print_info(cfg);
        return 0;
    }

    if (opts.interpreter_only) {
        return 0;
    }

    method* m = create_method(opts.method_id);
    if (m == NULL) {
        fprintf(stderr, "Methodid is not valid\n");
        return 3;
    }

    TSNode node;
    if (get_method_node(m, cfg, &node)) {
        fprintf(stderr, "Error while parsing code\n");

        delete_method(m);
        delete_config(cfg);
        return 4;
    }

    delete_method(m);
    delete_config(cfg);

    return 0;
}
