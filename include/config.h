#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    char* name;
    char* version;
    char* group;
    bool for_science;
    char* tags;
    char* jpamb_source_path;
    char* jpamb_decompiled_path;
} Config;

Config* config_load();
void config_delete(Config* cfg);
void config_print(const Config* cfg);

#endif
