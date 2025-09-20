#ifndef INFO_H
#define INFO_H

#include "config.h"

#include <stdbool.h>

bool is_info(const char* id);
void info_print(const Config* cfg);

#endif
