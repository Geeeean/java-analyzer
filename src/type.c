#include "type.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Type type_int = { .kind = TK_INT };
Type type_boolean = { .kind = TK_BOOLEAN };
Type type_reference = { .kind = TK_REFERENCE };
Type type_char = { .kind = TK_CHAR };
Type type_void = { .kind = TK_VOID };

Type* type_table = NULL;

static char args_type_signature[] = {
    [TK_INT] = 'I',
    [TK_CHAR] = 'C',
    [TK_ARRAY] = '[',
    [TK_BOOLEAN] = 'Z',
};

TypeKind get_tk(char c)
{
    if (c == args_type_signature[TK_INT]) {
        return TK_INT;
    } else if (c == args_type_signature[TK_CHAR]) {
        return TK_CHAR;
    } else if (c == args_type_signature[TK_ARRAY]) {
        return TK_ARRAY;
    } else if (c == args_type_signature[TK_BOOLEAN]) {
        return TK_BOOLEAN;
    }

    return -1;
}

Type* get_type(char** t)
{
    if (!t || !*t || **t == '\0') {
        LOG_ERROR("get_type: end of descriptor or null");
        return NULL;
    }

    char c = **t;
    Type* type = NULL;

    switch (c) {
    case 'I': // int
        (*t)++;               // consume 'I'
        type = TYPE_INT;
        break;

    case 'Z': // boolean
        (*t)++;               // consume 'Z'
        type = TYPE_BOOLEAN;
        break;

    case 'C': // char
        (*t)++;               // consume 'C'
        type = TYPE_CHAR;
        break;

    case 'V': // void (return type only)
        (*t)++;               // consume 'V'
        type = TYPE_VOID;
        break;

    case '[': // array
        (*t)++;               // consume '['
        type = make_array_type(get_type(t));
        break;

    case 'L': { // class / reference type: L...;
        (*t)++; // skip 'L'

        while (**t && **t != ';') {
            (*t)++;
        }

        if (**t == ';') {
            (*t)++; // consume the ';'
        }

        type = TYPE_REFERENCE;
        break;
    }

    default:
        LOG_ERROR("Unknown type kind while getting type: '%c' (0x%02X)",
                  c, (unsigned char)c);
        return NULL;
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
