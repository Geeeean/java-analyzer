#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>

typedef enum {
    TYPE_INT,
    TYPE_BOOLEAN,
    TYPE_REFERENCE,
    TYPE_CLASS,
    TYPE_CHAR,
    TYPE_ARRAY,
    TYPE_VOID,
} Type;

typedef struct {
    Type type;
    union {
        int int_value;
        bool bool_value;
        char char_value;
        struct ObjectType* ref;
    } data;
} PrimitiveType;

typedef struct {
    Type type;
    union {
        struct {
            Type* element_type;
            void* elements;
        } array;

        struct {
            char* class_name;
            //...
        } class;
    };
} ObjectType;


// Value value_deep_copy(const Value* src);

#endif
