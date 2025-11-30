#include "ir_program.h"
#include "log.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct IRItem IRItem;
struct IRItem {
    char* method_id;
    IrFunction* ir_function;
    Cfg* cfg;
    int num_locals;
    IRItem* next;
};

static IRItem* it_map = NULL;
static pthread_mutex_t ir_mutex = PTHREAD_MUTEX_INITIALIZER;

static IRItem* find_item(const char* id)
{
    IRItem* result = NULL;
    for (IRItem* it = it_map; it != NULL; it = it->next) {
        if (strcmp(id, it->method_id) == 0) {
            result = it;
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
    it->num_locals = vector_length(method_get_arguments_as_types(m));
    it->cfg = cfg_build(it->ir_function, it->num_locals);

    it->next = it_map;

    return it;
}

IrFunction* ir_program_get_function_ir(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);

    IrFunction* result = NULL;

#pragma omp critical(ir_program_map)
    {
        IRItem* item = find_item(id);
        if (item) {
            if (item->ir_function) {
                result = item->ir_function;
            } else {
                LOG_ERROR("item is defined but ir_function is not");
            }
        } else {
            IRItem* it = build_item(m, cfg);
            if (it) {
                it_map = it;
                result = it->ir_function;
            } else {
                LOG_ERROR("While building ir_program item");
            }
        }
    }

    return result;
}

Cfg* ir_program_get_cfg(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);

    Cfg* result = NULL;

#pragma omp critical(ir_program_map)
    {
        IRItem* item = find_item(id);
        if (item) {
            if (item->cfg) {
                result = item->cfg;
            } else {
                LOG_ERROR("item is defined but cfg not");
            }
        } else {
            IRItem* it = build_item(m, cfg);
            if (it) {
                it_map = it;
                result = it->cfg;
            } else {
                LOG_ERROR("While building ir_program item");
            }
        }
    }

    return result;
}

int ir_program_get_num_locals(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);

    IRItem* item = find_item(id);
    if (item) {
        return item->num_locals;
    }

    int result = -1;

#pragma omp critical(ir_program_map)
    {
        IRItem* item = find_item(id);
        if (item) {
            result = item->num_locals;
        } else {
            IRItem* it = build_item(m, cfg);
            if (it) {
                it_map = it;
                result = it->num_locals;
            }
        }
    }

    return result;
}

void ir_program_delete()
{
    IRItem* current = it_map;
    while (current != NULL) {
        IRItem* next_node = current->next;

        free(current->method_id);
        ir_function_delete(current->ir_function);
        cfg_delete(current->cfg);

        free(current);

        current = next_node;
    }

    it_map = NULL;
}
