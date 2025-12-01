#ifndef COMMON_H
#define COMMON_H

#include "vector.h"

#define SUCCESS 0
#define FAILURE 1

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

int min(Vector* v);

#endif
