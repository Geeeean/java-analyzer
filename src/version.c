#include "version.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define NAME "Gianluca Violoni"
#define VERSION "0.0"
#define GROUP "group 1"
#define FOR_SCIENCE 0
#define TAG_NUMBER 1
#define TAG1 "syntax"

#define INFO_ID "info"

bool is_info(char* id)
{
    return strcmp(id, INFO_ID) == 0;
}

// variable number of arguments used for tags
static void print_info_aux(char* name, char* version, char* group, bool for_science, int tags_number, ...)
{
    printf("%s\n", name);
    printf("%s\n", version);
    printf("%s\n", group);

    va_list tags;
    va_start(tags, tags_number);

    for (int i = 0; i < tags_number; i++) {
        char* tag = va_arg(tags, char*);
        printf("%s", tag);

        if (i < tags_number - 1) {
            printf(",");
        } else {
            printf("\n");
        }
    }

    if (for_science) {
        // TODO
    } else {
        printf("no\n");
    }

    va_end(tags);
}

void print_info()
{
    print_info_aux(NAME, VERSION, GROUP, FOR_SCIENCE, TAG_NUMBER, TAG1);
}
