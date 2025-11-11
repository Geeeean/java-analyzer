// TODO: handle delete (avoid mem leak)
#include "ir_program.h"

#include <string.h>

// todo: use hashmap
typedef struct IRItem IRItem;
struct IRItem {
    char* method_id;
    IrFunction* ir_function;
    Cfg* cfg;
    IRItem* next;
};

// to be freed
IRItem* it_map = NULL;

static IrFunction* find_function(const char* id)
{
    IrFunction* result = NULL;
    for (IRItem* it = it_map; it != NULL; it = it->next) {
        if (strcmp(id, it->method_id) == 0) {
            result = it->ir_function;
            break;
        }
    }

    return result;
}

static Cfg* find_cfg(const char* id)
{
    Cfg* result = NULL;
    for (IRItem* it = it_map; it != NULL; it = it->next) {
        if (strcmp(id, it->method_id) == 0) {
            result = it->cfg;
            break;
        }
    }

    return result;
}

static IRItem* build_item(const Method* m, const Config* cfg)
{
    if (!m || !cfg) {
        return NULL;
    }
    const char* id = method_get_id(m);
    if (!id) {
        return NULL;
    }

    IRItem* it = malloc(sizeof(IRItem));
    if (!it) {
        return NULL;
    }

    // TODO: check for success
    it->method_id = strdup(method_get_id(m));
    it->ir_function = ir_function_build(m, cfg);
    it->cfg = cfg_build(it->ir_function);

    it->next = it_map;

    return it;
}

IrFunction* ir_program_get_function_ir(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);

    // First check: no lock (fast)
    IrFunction* result = find_function(id);
    if (result) {
        return result;
    }

#pragma omp critical(ir_program_map)
    {
        // Second check: with lock (avoid double creation)
        result = find_function(id);

        if (!result) {
            IRItem* it = build_item(m, cfg);
            if (it) {
                it_map = it;
                result = it->ir_function;
            }
        }
    }

    return result;
}

Cfg* ir_program_get_cfg(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);

    // First check: no lock (fast)
    Cfg* result = find_cfg(id);
    if (result) {
        return result;
    }

#pragma omp critical(ir_program_map)
    {
        // Second check: with lock (avoid double creation)
        result = find_cfg(id);

        if (!result) {
            IRItem* it = build_item(m, cfg);
            if (it) {
                it_map = it;
                result = it->cfg;
            }
        }
    }

    return result;
}
