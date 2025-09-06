#ifndef METHOD_H
#define METHOD_H

#include "config.h"

typedef struct method method;

method* create_method(char* method_id);
void delete_method(method* m);
void print_method(const method* m);
char* read_method_source(const method* m, const config* cfg);

#endif
