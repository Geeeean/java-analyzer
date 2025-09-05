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
    char* jpamb_path;
} config;

config* load_config();
void delete_config(config* cfg);
void print_config(const config* cfg);

#endif
