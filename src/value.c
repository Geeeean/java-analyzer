#include "value.h"
#include "log.h"

BinaryResult value_add(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    if (value1->type == TYPE_INT) {
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value + value2->data.int_value;
    } else {
        LOG_ERROR("Dont know handle this type add");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_mul(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    if (value1->type == TYPE_INT) {
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value * value2->data.int_value;
    } else {
        LOG_ERROR("Dont know handle this type mul");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_sub(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    if (value1->type == TYPE_INT) {
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value - value2->data.int_value;
    } else {
        LOG_ERROR("Dont know handle this type sub");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_rem(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    if (value1->type == TYPE_INT) {
        result->type = TYPE_INT;
        result->data.int_value = value1->data.int_value % value2->data.int_value;
    } else {
        LOG_ERROR("Dont know handle this type sub");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}

BinaryResult value_div(Value* value1, Value* value2, Value* result)
{
    if (value1->type != value2->type) {
        return BO_DIFFERENT_TYPES;
    }

    if (value1->type == TYPE_INT) {
        result->type = TYPE_INT;
        if (value2->data.int_value == 0) {
            return BO_DIVIDE_BY_ZERO;
        }

        result->data.int_value = value1->data.int_value / value2->data.int_value;
    } else {
        LOG_ERROR("Dont know handle this type div");
        return BO_NOT_SUPPORTED_TYPES;
    }

    return BO_OK;
}
