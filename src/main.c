#include "config.h"
#include "info.h"
#include "method.h"
#include "tree_sitter/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const TSLanguage* tree_sitter_java(void);

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Correct usage: ./analyzer <methodid>\n");
        return 1;
    }

    config* cfg = load_config();
    if (cfg == NULL) {
        printf("Config file is wrongly formatted or not exist\n");
        return 1;
    }

    char* method_id = argv[1];
    if (is_info(method_id)) {
        print_info(cfg);
        return 0;
    }

    method* m = create_method(method_id);
    if (m == NULL) {
        printf("methodid is not valid\n");
        return 1;
    }

    char* source = read_method_source(m, cfg);

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_java());

    TSTree* tree = ts_parser_parse_string(
        parser,
        NULL,
        source,
        strlen(source));

    TSNode root_node = ts_tree_root_node(tree);
    char* string = ts_node_string(root_node);
    printf("Syntax tree: %s\n", string);

    free(string);
    free(source);
    delete_method(m);
    delete_config(cfg);

    return 0;
}
