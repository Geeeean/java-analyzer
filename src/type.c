#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Type type_int = { .kind = TK_INT };
Type type_boolean = { .kind = TK_BOOLEAN };
Type type_reference = { .kind = TK_REFERENCE };
Type type_char = { .kind = TK_CHAR };
Type type_void = { .kind = TK_VOID };

Type* type_table = NULL;

Type* make_array_type(Type* element_type)
{
    for (Type* t = type_table; t != NULL; t = t->next) {
        if (t->kind == TK_ARRAY && t->array.element_type == element_type)
            return t;
    }

    Type* t = malloc(sizeof(Type));
    t->kind = TK_ARRAY;
    t->array.element_type = element_type;
    t->next = type_table;
    type_table = t;

    return t;
}

// Type* make_class_type(const char* name)
//{
//     for (Type* t = type_table; t != NULL; t = t->next) {
//         if (t->kind == TYPE_CLASS && strcmp(t->class.class_name, name) == 0)
//             return t;
//     }
//
//     Type* t = malloc(sizeof(Type));
//     t->kind = TYPE_CLASS;
//     t->class.class_name = name;
//     t->next = type_table;
//     type_table = t;
//
//     return t;
// }
