#include "ir_program.h"

#include <string.h>

// todo: use hashmap
typedef struct IRItem IRItem;
struct IRItem {
    char* method_id;
    IrFunction* ir_function;
    IRItem* next;
};

// to be freed
IRItem* it_map = NULL;

IrFunction* ir_program_get_function(const Method* m, const Config* cfg)
{
    for (IRItem* it = it_map; it != NULL; it = it->next) {
        if (strcmp(method_get_id(m), it->method_id) == 0) {
            return it->ir_function;
        }
    }

    IRItem* it = malloc(sizeof(IRItem));
    it->ir_function = ir_function_build(m, cfg);
    it->next = it_map;
    it->method_id = strdup(method_get_id(m));

    return it->ir_function;
}
