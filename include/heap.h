#ifndef HEAP_H
#define HEAP_H

#include "value.h"

int heap_insert(ObjectValue* obj, int* index);
ObjectValue* heap_get(int index);

#endif
