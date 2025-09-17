#include "info.h"

#include <stdio.h>
#include <string.h>

#define INFO_ID "info"

bool is_info(const char* id)
{
    return strcmp(id, INFO_ID) == 0;
}

// variable number of arguments used for tags
static void print_info_aux(char* name, char* version, char* group, bool for_science, char* tags)
{
    printf("%s\n", name);
    printf("%s\n", version);
    printf("%s\n", group);
    printf("%s\n", tags);

    if (for_science) {
        // TODO
    } else {
        printf("no\n");
    }
}

void print_info(const config* cfg)
{
    print_info_aux(cfg->name, cfg->version, cfg->group, cfg->for_science, cfg->tags);
}
