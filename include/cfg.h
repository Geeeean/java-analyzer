#ifndef CFG_H
#define CFG_H

#include <ir_function.h>

typedef struct {
    Vector* blocks;
} Cfg;

typedef struct {
    int id;
    int og_id;
    int ip_start;
    int ip_end;
    Vector* successors;
    IrFunction* ir_function;
    int num_locals;
} BasicBlock;

Cfg* cfg_build(IrFunction* ir_function, int num_locals);
void cfg_print(Cfg* cfg);

int cfg_inline(Cfg* cfg, Config* config, Method* m);

#endif
