#include "type.h"

// todo use return as status code
// Value value_deep_copy(const Value* src)
//{
//    Value dst;
//    dst.type = -1;
//
//    if (!src)
//        return dst;
//
//    dst.type = src->type;
//
//    switch (src->type) {
//    case TYPE_INT:
//        dst.data.int_value = src->data.int_value;
//        break;
//
//    case TYPE_BOOLEAN:
//        dst.data.bool_value = src->data.bool_value;
//        break;
//
//    case TYPE_CHAR:
//        dst.data.char_value = src->data.char_value;
//        break;
//
//    case TYPE_ARRAY:
//        if (src->data.array_value) {
//            int count = 0;
//            while (src->data.array_value[count].type != TYPE_VOID) {
//                count++;
//            }
//
//            dst.data.array_value = malloc((count + 1) * sizeof(Value));
//            if (!dst.data.array_value) {
//                LOG_ERROR("Failed to allocate array");
//                dst.type = TYPE_VOID;
//                break;
//            }
//
//            for (int i = 0; i < count; i++) {
//                dst.data.array_value[i] = value_deep_copy(&src->data.array_value[i]);
//            }
//
//            dst.data.array_value[count].type = TYPE_VOID;
//        } else {
//            dst.data.array_value = NULL;
//        }
//        break;
//
//    case TYPE_REFERENCE:
//        dst.data.ref_value = src->data.ref_value;
//        break;
//
//    case TYPE_VOID:
//        break;
//
//    default:
//        LOG_ERROR("Unknown value type: %d", src->type);
//        dst.type = TYPE_VOID;
//        break;
//    }
//
//    return dst;
//}
