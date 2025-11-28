#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#if DEBUG
#define LOG_DEBUG(msg, ...) fprintf(stderr, "[DEBUG] " msg "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...)
#endif

#ifdef NO_PRINT
#define LOG_TODO(msg, ...)
#define LOG_INFO(msg, ...)
#define LOG_ERROR(msg, ...)
#else
#define LOG_TODO(msg, ...) fprintf(stderr, "[TODO] " msg "\n", ##__VA_ARGS__)
#define LOG_INFO(msg, ...) fprintf(stderr, "[INFO] " msg "\n", ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) fprintf(stderr, "[ERROR] " msg "\n", ##__VA_ARGS__)
#endif

#define LOG_BENCHMARK(msg, ...) fprintf(stderr, "[BENCHMARK] " msg "\n", ##__VA_ARGS__)

#endif
