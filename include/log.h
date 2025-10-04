#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#if DEBUG
#define LOG_DEBUG(msg, ...) fprintf(stderr, "[DEBUG] " msg "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...)
#endif

#define LOG_INFO(msg, ...) fprintf(stdout, "[INFO] " msg "\n", ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) fprintf(stderr, "[ERROR] " msg "\n", ##__VA_ARGS__)

#endif
