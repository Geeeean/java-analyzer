#include "type.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Type type_int = { .kind = TK_INT };
Type type_boolean = { .kind = TK_BOOLEAN };
Type type_reference = { .kind = TK_REFERENCE };
Type type_char = { .kind = TK_CHAR };
Type type_double = { .kind = TK_DOUBLE };
Type type_void = { .kind = TK_VOID };

Type* type_table = NULL;

static char args_type_signature[] = {
    [TK_INT] = 'I',
    [TK_CHAR] = 'C',
    [TK_DOUBLE] = 'D',
    [TK_ARRAY] = '[',
    [TK_BOOLEAN] = 'Z',
};

TypeKind get_tk(char c)
{
    if (c == args_type_signature[TK_INT]) {
        return TK_INT;
    } else if (c == args_type_signature[TK_CHAR]) {
        return TK_CHAR;
    } else if (c == args_type_signature[TK_DOUBLE]) {
        return TK_DOUBLE;
    } else if (c == args_type_signature[TK_ARRAY]) {
        return TK_ARRAY;
    } else if (c == args_type_signature[TK_BOOLEAN]) {
        return TK_BOOLEAN;
    }

    return -1;
}

Type* get_type(char** t)
{
    Type* type = NULL;

    TypeKind tk = get_tk(**t);
    switch (tk) {
    case TK_INT:
        type = TYPE_INT;
        break;
    case TK_BOOLEAN:
        type = TYPE_BOOLEAN;
        break;
    case TK_REFERENCE:
        type = TYPE_REFERENCE;
        break;
    case TK_CLASS:
        LOG_ERROR("TODO: get type class in type.c: 'get_type'");
        break;
    case TK_CHAR:
        type = TYPE_CHAR;
        break;
    case TK_DOUBLE:
        type = TYPE_DOUBLE;
        break;
    case TK_ARRAY:
        (*t)++;
        type = make_array_type(get_type(t));
        break;
    case TK_VOID:
        type = TYPE_VOID;
        break;
    default:
        LOG_ERROR("Unknown type kind while getting type");
        break;
    }

    return type;
}

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

bool type_is_array(const Type* type)
{
    return type->kind == TK_ARRAY;
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
