#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdio.h>

typedef struct Config Config;

Config* config_load();
void config_delete(Config* cfg);
void config_print(const Config* cfg);
char* config_get_name(const Config* cfg);
char* config_get_version(const Config* cfg);
char* config_get_group(const Config* cfg);
bool config_get_for_science(const Config* cfg);
char* config_get_tags(const Config* cfg);
char* config_get_decompiled(const Config* cfg);
char* config_get_source(const Config* cfg);

#endif
