#ifndef VALUE_H
#define VALUE_H

#include <type.h>

typedef enum {
    BO_OK,
    BO_DIFFERENT_TYPES,
    BO_NOT_SUPPORTED_TYPES,
    BO_DIVIDE_BY_ZERO,
} BinaryResult;

typedef struct {
    Type* type;
    union {
        int int_value;
        bool bool_value;
        char char_value;
        double double_value;
        int ref_value;
    } data;
} Value;

typedef struct {
    Type* type;
    union {
        struct {
            int elements_count;
            Value* elements;
        } array;

        // struct {
        //     int fields_count;
        //     struct Value* fields;
        // } class;
    };
} ObjectValue;

BinaryResult value_add(Value* value1, Value* value2, Value* result);
BinaryResult value_mul(Value* value1, Value* value2, Value* result);
BinaryResult value_sub(Value* value1, Value* value2, Value* result);
BinaryResult value_rem(Value* value1, Value* value2, Value* result);
BinaryResult value_div(Value* value1, Value* value2, Value* result);

#endif
