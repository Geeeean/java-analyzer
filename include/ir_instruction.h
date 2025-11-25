#ifndef IR_INSTRUCTION_H
#define IR_INSTRUCTION_H

#include "opcode.h"

typedef struct {
    Opcode opcode;
    int seq;
    union {
        PushOP push;
        LoadOP load;
        BinaryOP binary;
        ReturnOP ret;
        GetOP get;
        InvokeOP invoke;
        IfOP ift;
        ThrowOP trw;
        StoreOP store;
        GotoOP go2;
        CastOP cast;
        DupOP dup;
        NewArrayOP new_array;
        ArrayStoreOP array_store;
        ArrayLoadOP array_load;
        ArrayLengthOP array_length;
        IncrOP incr;
        NegateOP negate;
        CompareFloatingOP compare_floating;
    } data;
} IrInstruction;

IrInstruction*
ir_instruction_parse(cJSON* instruction_json);
int ir_instruction_is_conditional(IrInstruction* ir_instruction);

#endif
