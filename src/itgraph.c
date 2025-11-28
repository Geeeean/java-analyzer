#include "itgraph.h"
#include "interpreter.h"
#include "opcode.h"
#include "vector.h"
#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    char* sig;
    Method* method;
} MethodEntry;

static MethodEntry* method_cache = NULL;
static size_t method_cache_count = 0;

static Method* intern_method(const char* sig)
{
    // search existing
    for (size_t i = 0; i < method_cache_count; i++) {
        if (strcmp(method_cache[i].sig, sig) == 0)
            return method_cache[i].method;
    }

    // create new
    Method* m = method_create(sig);
    if (!m) return NULL;

    method_cache = realloc(method_cache,
                           sizeof(MethodEntry) * (method_cache_count + 1));

    method_cache[method_cache_count].sig = strdup(sig);
    method_cache[method_cache_count].method = m;
    method_cache_count++;

    return m;
}

static bool seen_before(const Method* m, Vector* seen)
{
    for (size_t i = 0; i < vector_length(seen); i++) {
        const Method* x = *(const Method**) vector_get(seen, i);
        if (x == m) return true;
    }
    return false;
}


static void dfs_collect(const Method* m,
                        const Config* cfg,
                        Vector* out,
                        Vector* seen)
{
    if (!m) return;

    if (seen_before(m, seen))
        return;

    vector_push(seen, &m);
    vector_push(out, &m);

    InstructionTable* t = instruction_table_build(m, cfg);
    if (!t) return;

    for (int pc = 0; pc < t->count; pc++) {
        Instruction* inst = t->instructions[pc];
        if (!inst) continue;

        if (inst->opcode == OP_INVOKE) {
            InvokeOP* iv = &inst->data.invoke;
            if (!iv || !iv->ref_name || !iv->method_name)
                continue;

            char* sig = get_method_signature(iv);
            if (!sig) continue;

            // Deduplicate and keep stable Method pointer
            Method* callee = intern_method(sig);
            free(sig);

            if (!callee) continue;

            dfs_collect(callee, cfg, out, seen);
        }
    }
}


Vector* resolve_all_methods_reachable_from(const Method* root,
                                           const Config* cfg)
{
    Vector* out = vector_new(sizeof(Method*));
    Vector* seen = vector_new(sizeof(Method*));

    dfs_collect(root, cfg, out, seen);

    vector_delete(seen);
    return out;
}

void itgraph_free_all(void)
{
    for (size_t i = 0; i < method_cache_count; i++) {
        free(method_cache[i].sig);
        method_delete(method_cache[i].method);
    }
    free(method_cache);
    method_cache = NULL;
    method_cache_count = 0;
}
