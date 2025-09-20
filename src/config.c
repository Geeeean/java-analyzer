#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define APP_NAME "java-analyzer"
#define CONFIG_FILE "java-analyzer.conf"
#define LINE_SIZE 256
#define LINE_SEP " "

#define PWD_MAX 256
#define CONFIG_PATH_MAX 256

struct Config {
    char* name;
    char* version;
    char* group;
    bool for_science;
    char* tags;
    char* jpamb_source_path;
    char* jpamb_decompiled_path;
};

char* config_get_name(const Config* cfg)
{
    return cfg->name;
}

char* config_get_version(const Config* cfg)
{
    return cfg->version;
}

char* config_get_group(const Config* cfg)
{
    return cfg->group;
}

bool config_get_for_science(const Config* cfg)
{
    return cfg->for_science;
}

char* config_get_tags(const Config* cfg)
{
    return cfg->tags;
}

char* config_get_decompiled(const Config* cfg)
{
    return cfg->jpamb_decompiled_path;
}

char* config_get_source(const Config* cfg)
{
    return cfg->jpamb_source_path;
}

static int set_field(Config* cfg, char* line)
{
    char* key = strtok(line, LINE_SEP);
    char* value = strtok(NULL, " \n");

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
    } else if (strcmp(key, "jpamb_source_path") == 0) {
        cfg->jpamb_source_path = strdup(value);
    } else if (strcmp(key, "jpamb_decompiled_path") == 0) {
        cfg->jpamb_decompiled_path = strdup(value);
    } else {
        return 1;
    }

    return 0;
}

static int sanity_check(const Config* cfg)
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

    if (cfg->jpamb_source_path == NULL) {
        return 6;
    }

    if (cfg->jpamb_decompiled_path == NULL) {
        return 6;
    }

    return 0;
}

static char* get_user_config_path(const char* appname, const char* filename)
{
    const char* xdg = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");

    static char path[CONFIG_PATH_MAX];

    if (xdg && *xdg) {
        snprintf(path, sizeof(path), "%s/%s/%s", xdg, appname, filename);
    } else if (home && *home) {
        snprintf(path, sizeof(path), "%s/.config/%s/%s", home, appname, filename);
    } else {
        return NULL;
    }

    return path;
}

Config* config_load()
{
    char* path = get_user_config_path(APP_NAME, CONFIG_FILE);
    if (!path) {
        return NULL;
    }

    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return NULL;
    }

    Config* cfg = malloc(sizeof(Config));

    char buffer[LINE_SIZE];
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        if (set_field(cfg, buffer)) {
            // fprintf(stderr, "Error while checking config structure: %d\n", check);

            config_delete(cfg);
            fclose(f);
            return NULL;
        }
    }

    fclose(f);

    int check = sanity_check(cfg);
    if (check) {
        fprintf(stderr, "Error while checking config structure: %d\n", check);
        config_delete(cfg);
        return NULL;
    }

    return cfg;
}

void config_delete(Config* cfg)
{
    if (cfg) {
        free(cfg->name);
        free(cfg->version);
        free(cfg->group);
        free(cfg->tags);
        free(cfg->jpamb_source_path);
    }

    free(cfg);
}

void config_print(const Config* cfg)
{
    int check = sanity_check(cfg);
    if (check) {
        fprintf(stderr, "Error while checking config structure: %d\n", check);
        return;
    }

    printf("analyzer name:                   %s\n", cfg->name);
    printf("analyzer version:                %s\n", cfg->version);
    printf("analyzer group:                  %s\n", cfg->group);
    printf("analyzer for_science:            %d\n", cfg->for_science);
    printf("analyzer tags:                   %s\n", cfg->tags);
    printf("analyzer jpamb_source_path:      %s\n", cfg->jpamb_source_path);
    printf("analyzer jpamb_decompiler_path:  %s\n", cfg->jpamb_decompiled_path);
}
