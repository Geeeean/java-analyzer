#include "syntax.h"

#include <stdio.h>
#include <string.h>

#define METHOD_NAME_SIZE 256

const TSLanguage* tree_sitter_java(void);

TSTree* syntax_tree_build(Method* m, Config* cfg)
{

    TSParser* parser = ts_parser_new();
    if (!ts_parser_set_language(parser, tree_sitter_java())) {
        return NULL;
    }

    if (!parser) {
        return NULL;
    }

    char* source = method_read(m, cfg, SRC_SOURCE);
    if (!source) {
        ts_parser_delete(parser);
        return NULL;
    }

    TSTree* tree = ts_parser_parse_string(
        parser,
        NULL,
        source,
        strlen(source));

    ts_parser_delete(parser);
    free(source);

    return tree;
}

static TSQuery* get_query(const char* query_src)
{
    uint32_t error_offset;
    TSQueryError error_type;

    return ts_query_new(
        tree_sitter_java(),
        query_src,
        strlen(query_src),
        &error_offset,
        &error_type);
}

static TSQueryCursor* run_query(TSNode root, TSQuery* query)
{
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, root);

    return cursor;
}

static int find_method(TSNode root, char* source, const char* query_src, const char* to_find, TSNode* method_node)
{
    TSQuery* query = get_query(query_src);
    if (!query) {
        return 1;
    }

    TSQueryCursor* cursor = run_query(root, query);

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint32_t i = 0; i < match.capture_count; i++) {
            TSQueryCapture capture = match.captures[i];
            uint32_t len;
            const char* name = ts_query_capture_name_for_id(query, capture.index, &len);

            if (strcmp(name, "method.name") == 0) {
                TSNode node = capture.node;
                int len = ts_node_end_byte(node) - ts_node_start_byte(node);

                char* method_name = malloc(sizeof(char) * len + 1);
                strncpy(method_name, source + ts_node_start_byte(node), len);
                method_name[len] = '\0';

                if (strcmp(method_name, to_find) == 0) {
                    free(method_name);
                    ts_query_cursor_delete(cursor);
                    ts_query_delete(query);

                    *method_node = capture.node;
                    return 0;
                }

                free(method_name);
            }
        }
    }

    ts_query_cursor_delete(cursor);
    ts_query_delete(query);

    return 2;
}

int method_node_get(TSTree* tree, TSNode* node)
{
    TSNode root = ts_tree_root_node(tree);

    const char* query_src = "(method_declaration "
                            "      name: (identifier) @method.name)";

    // TODO

    return 0;
}
