#ifndef UTILS_H
#define UTILS_H

#include "opcode.h"

char* get_method_signature(InvokeOP* invoke);
void replace_char(char* str, char find, char replace);

double get_current_time();

#endif
