#ifndef CFG_H
#define CFG_H

#include <ir_function.h>

typedef struct {
    int id;
    int ip_start;
    int ip_end;
    Vector* successors;
} BasicBlock;

typedef struct {
    Vector* blocks;
} Cfg;

Cfg* cfg_build(IrFunction* ir_function);
void cfg_print(Cfg* cfg);

#endif
