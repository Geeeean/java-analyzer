// ir_program.c

#include "ir_program.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct IRItem IRItem;
struct IRItem {
    const Method* method;   // key: pointer identity
    IrFunction*   ir_function;
    IRItem*       next;
};

static IRItem* it_map = NULL;
static pthread_mutex_t ir_mutex = PTHREAD_MUTEX_INITIALIZER;

IrFunction* ir_program_get_function(const Method* m, const Config* cfg)
{
    if (!m || !cfg) {
        LOG_ERROR("ir_program_get_function: called with m=%p cfg=%p",
                  (void*)m, (void*)cfg);
        return NULL;
    }

    pthread_mutex_lock(&ir_mutex);

    // 1) Look for existing entry by Method* identity
    for (IRItem* it = it_map; it; it = it->next) {
        if (it->method == m) {
            IrFunction* fn = it->ir_function;
            pthread_mutex_unlock(&ir_mutex);
            return fn;
        }
    }

    // 2) Not found -> build new
    IRItem* new_item = malloc(sizeof(IRItem));
    if (!new_item) {
        pthread_mutex_unlock(&ir_mutex);
        return NULL;
    }

    new_item->method = m;
    new_item->ir_function = ir_function_build(m, cfg);
    if (!new_item->ir_function) {
        free(new_item);
        pthread_mutex_unlock(&ir_mutex);
        return NULL;
    }

    // 3) Insert into list
    new_item->next = it_map;
    it_map = new_item;

    IrFunction* fn = new_item->ir_function;
    pthread_mutex_unlock(&ir_mutex);
    return fn;
}

void ir_program_free_all(void)
{
    pthread_mutex_lock(&ir_mutex);

    IRItem* it = it_map;
    while (it) {
        IRItem* next = it->next;

        if (it->ir_function) {
            ir_function_delete(it->ir_function);
            it->ir_function = NULL;
        }

        free(it);
        it = next;
    }

    it_map = NULL;

    pthread_mutex_unlock(&ir_mutex);
}
