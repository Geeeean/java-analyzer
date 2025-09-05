#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE "analyzer.conf"
#define LINE_SIZE 256
#define LINE_SEP " "

static void
cleanup_method_partial(config* cfg)
{
    if (cfg->name != NULL) {
        free(cfg->name);
    }

    if (cfg->version != NULL) {
        free(cfg->version);
    }

    if (cfg->group != NULL) {
        free(cfg->group);
    }

    if (cfg->tags != NULL) {
        free(cfg->tags);
    }

    if (cfg->jpamb_path != NULL) {
        free(cfg->jpamb_path);
    }
}

static int set_field(config* cfg, char* line)
{
    char* key = strtok(line, LINE_SEP);
    char* value = strtok(NULL, "\n");

    if (!key || !value)
        return 1;

    if (strcmp(key, "name") == 0) {
        cfg->name = strdup(value);
    } else if (strcmp(key, "version") == 0) {
        cfg->version = strdup(value);
    } else if (strcmp(key, "group") == 0) {
        cfg->group = strdup(value);
    } else if (strcmp(key, "for_science") == 0) {
        cfg->for_science = strcmp(key, "1") == 0;
    } else if (strcmp(key, "tags") == 0) {
        cfg->tags = strdup(value);
    } else if (strcmp(key, "jpamb_path") == 0) {
        cfg->jpamb_path = strdup(value);
    } else {
        return 1;
    }

    return 0;
}

static int sanity_check(const config* cfg)
{
    if (cfg->name == NULL) {
        return 1;
    }

    if (cfg->version == NULL) {
        return 2;
    }

    if (cfg->group == NULL) {
        return 3;
    }

    if (cfg->tags == NULL) {
        return 5;
    }

    if (cfg->jpamb_path == NULL) {
        return 6;
    }

    return 0;
}

config* load_config()
{
    FILE* f = fopen(CONFIG_FILE, "r");
    if (f == NULL) {
        return NULL;
    }

    config* cfg = malloc(sizeof(config));

    char buffer[LINE_SIZE];
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        if (set_field(cfg, buffer)) {
            delete_config(cfg);
            return NULL;
        }
    }

    if (sanity_check(cfg)) {
        delete_config(cfg);
        return NULL;
    }

    return cfg;
}

void delete_config(config* cfg)
{
    if (cfg == NULL) {
        return;
    }

    cleanup_method_partial(cfg);
    free(cfg);
}

void print_config(const config* cfg)
{
    if (sanity_check(cfg)) {
        return;
    }

    printf("analyzer name:        %s\n", cfg->name);
    printf("analyzer version:     %s\n", cfg->version);
    printf("analyzer group:       %s\n", cfg->group);
    printf("analyzer for_science: %d\n", cfg->for_science);
    printf("analyzer tags:        %s\n", cfg->tags);
    printf("analyzer jpamb_path:  %s\n", cfg->jpamb_path);
}
