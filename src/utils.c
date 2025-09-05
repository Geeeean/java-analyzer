#include "utils.h"

#include <string.h>

void replace_char(char* str, char find, char replace)
{
    char* pos = str;

    while ((pos = strchr(pos, find)) != NULL) {
        *pos = replace;
        pos++;
    }
}
