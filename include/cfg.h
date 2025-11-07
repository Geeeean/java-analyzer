#ifndef CFG_H
#define CFG_H

#include <ir_function.h>

typedef struct Cfg Cfg;

Cfg* cfg_build(IrFunction* ir_function);

#endif
