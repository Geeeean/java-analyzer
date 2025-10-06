#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>

typedef enum {
    TK_INT,
    TK_BOOLEAN,
    TK_REFERENCE,
    TK_CLASS,
    TK_CHAR,
    TK_ARRAY,
    TK_VOID,
} TypeKind;

typedef struct Type Type;
struct Type {
    TypeKind kind;
    union {
        struct {
            struct Type* element_type;
        } array;

        // struct {
        //     const char* class_name;
        //     int field_count;
        //     struct Type** field_types;
        // } class;
    };
    Type* next;
};

extern Type type_int;
extern Type type_boolean;
extern Type type_reference;
extern Type type_char;
extern Type type_void;

#define TYPE_INT &type_int
#define TYPE_BOOLEAN &type_boolean
#define TYPE_REFERENCE &type_reference
#define TYPE_CHAR &type_char
#define TYPE_VOID &type_void

Type* make_array_type(Type* element_type);
Type* make_class_type(const char* name);

#endif
