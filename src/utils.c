#include "utils.h"

#include "time.h"
#include <string.h>

void replace_char(char* str, char find, char replace)
{
    char* pos = str;

    while ((pos = strchr(pos, find)) != NULL) {
        *pos = replace;
        pos++;
    }
}

double get_current_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
}
