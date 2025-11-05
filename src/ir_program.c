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
    const char* id = method_get_id(m);

    // First check: no lock (fast)
    for (IRItem* it = it_map; it != NULL; it = it->next) {
        if (strcmp(id, it->method_id) == 0) {
            return it->ir_function;
        }
    }

    IrFunction* result = NULL;

#pragma omp critical(ir_program_map)
    {
        // Second check: with lock (avoid double creation)
        for (IRItem* it = it_map; it != NULL; it = it->next) {
            if (strcmp(id, it->method_id) == 0) {
                result = it->ir_function;
                goto done;
            }
        }

        IRItem* it = malloc(sizeof(IRItem));
        it->method_id = strdup(id);
        it->ir_function = ir_function_build(m, cfg);

        it->next = it_map;
        it_map = it;

        result = it->ir_function;
    }

done:
    return result;
}
