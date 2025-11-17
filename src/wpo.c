#include "cfg.h"
#include "scc.h"
#include "vector.h"
#include <limits.h>

void* wpo(Cfg* cfg)
{
    if (!cfg) {
        return NULL;
    }

    CondensedSCC* cscc = cscc_build(cfg);
    if (!cscc) {
        goto cleanup;
    }

cleanup:
    return NULL;
}
