#include "domain.h"
#include "common.h"
#include "domain_interval.h"

static int is_valid_domain(Domain domain)
{
    return domain >= 0 && domain < DOMAIN_COUNT;
}

int join(Domain domain, void* state_accumulator, void* state_new)
{
    if (!is_valid_domain(domain)) {
        return FAILURE;
    }

    return SUCCESS;
}

int intersection(Domain domain, void* state_accumulator, void* state_constraint);

int widening(Domain domain, void* state_accumulator, void* state_new);
int narrowing(Domain domain, void* state_accumulator, void* state_prev);

int transfer_assignment(Domain domain, void* in_state, void* out_state, int dst, int src1, int src2);
int transfer_mul(Domain domain, void* in_state, void* out_state, int dst, int src1, int src2);
int transfer_div(Domain domain, void* in_state, void* out_state, int dst, int src1, int src2);
int transfer_sum(Domain domain, void* in_state, void* out_state, int dst, int src1, int src2);
int transfer_sub(Domain domain, void* in_state, void* out_state, int dst, int src1, int src2);
