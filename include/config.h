#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef struct Config {
  char* name;
  char* version;
  char* group;
  bool  for_science;
  char* tags;
  char* jpamb_source_path;
  char* jpamb_decompiled_path;
  int   threads;
  bool threads_set;
} Config;

Config* config_load();
void config_delete(Config* cfg);
void config_print(const Config* cfg);

// Accessors
char* config_get_name(const Config* cfg);
char* config_get_version(const Config* cfg);
char* config_get_group(const Config* cfg);
bool  config_get_for_science(const Config* cfg);
char* config_get_tags(const Config* cfg);
char* config_get_decompiled(const Config* cfg);
char* config_get_source(const Config* cfg);

#endif
