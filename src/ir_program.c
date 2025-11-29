// ir_program.c
#include "ir_program.h"
#include "ir_function.h"
#include "cfg.h"
#include "method.h"
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


IrFunction* ir_program_get_function(const Method* m, const Config* cfg)
{
    return ir_program_get_function_ir(m, cfg);
}

static IRItem* find_item(const char* id)
{
    for (IRItem* it = it_map; it; it = it->next) {
        if (strcmp(it->method_id, id) == 0)
            return it;
    }
    return NULL;
}

static IRItem* build_item(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);
    if (!id) return NULL;

    IRItem* it = calloc(1, sizeof(IRItem));
    if (!it) return NULL;

    it->method_id = strdup(id);
    if (!it->method_id) {
        free(it);
        return NULL;
    }

    it->ir_function = ir_function_build(m, cfg);
    if (!it->ir_function) {
        free(it->method_id);
        free(it);
        return NULL;
    }

    Vector* types = method_get_arguments_as_types(m);
    it->num_locals = (int)vector_length(types);
    vector_delete(types);

    it->cfg = cfg_build(it->ir_function, it->num_locals);

    return it;
}


IrFunction* ir_program_get_function_ir(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);
    if (!id) return NULL;

    IrFunction* result = NULL;

#pragma omp critical(ir_program_map)
    {
        IRItem* it = find_item(id);
        if (it) {
            result = it->ir_function;
        } else {
            IRItem* new_item = build_item(m, cfg);
            if (new_item) {
                new_item->next = it_map;
                it_map = new_item;
                result = new_item->ir_function;
            }
        }
    }

    return result;
}

Cfg* ir_program_get_cfg(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);
    if (!id) return NULL;

    Cfg* result = NULL;

#pragma omp critical(ir_program_map)
    {
        IRItem* it = find_item(id);
        if (it) {
            result = it->cfg;
        } else {
            IRItem* new_item = build_item(m, cfg);
            if (new_item) {
                new_item->next = it_map;
                it_map = new_item;
                result = new_item->cfg;
            }
        }
    }

    return result;
}

int ir_program_get_num_locals(const Method* m, const Config* cfg)
{
    const char* id = method_get_id(m);
    if (!id) return -1;

    int result = -1;

#pragma omp critical(ir_program_map)
    {
        IRItem* it = find_item(id);
        if (it) {
            result = it->num_locals;
        } else {
            IRItem* new_item = build_item(m, cfg);
            if (new_item) {
                new_item->next = it_map;
                it_map = new_item;
                result = new_item->num_locals;
            }
        }
    }

    return result;
}


void ir_program_delete(void)
{
#pragma omp critical(ir_program_map)
    {
        IRItem* it = it_map;
        it_map = NULL;

        while (it) {
            IRItem* next = it->next;

            free(it->method_id);
            ir_function_delete(it->ir_function);
            cfg_delete(it->cfg);

            free(it);
            it = next;
        }
    }
}

/* Required by main.c and fuzzer.c */
void ir_program_free_all(void)
{
    ir_program_delete();
}