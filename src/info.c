#include "info.h"

#include <log.h>
#include <stdio.h>
#include <string.h>

#define INFO_ID "info"

bool is_info(const char* id)
{
    return strcmp(id, INFO_ID) == 0;
}

// variable number of arguments used for tags
static void info_print_aux(char* name, char* version, char* group, bool for_science, char* tags)
{
    printf("%s\n", name);
    printf("%s\n", version);
    printf("%s\n", group);
    printf("%s\n", tags);

    if (for_science) {
        LOG_TODO("for science in info print (info.c)");
    } else {
        printf("no\n");
    }
}

void info_print(const Config* cfg)
{
    info_print_aux(
        config_get_name(cfg),
        config_get_version(cfg),
        config_get_group(cfg),
        config_get_for_science(cfg),
        config_get_tags(cfg));
}
