#ifndef METHOD_H
#define METHOD_H

typedef struct method method;

method* create_method(char* method_id);
void delete_method(method* m);
void print_method(const method* m);

#endif
