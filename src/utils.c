#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils.h"
#include "opcode.h"
#include "type.h"
#include "log.h"

void replace_char(char* s, char from, char to)
{
  if (!s) return;
  while (*s) {
    if (*s == from) *s = to;
    s++;
  }
}