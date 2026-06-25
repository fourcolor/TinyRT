#pragma once

#include "critical.h"
#include "rtos_config.h"

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

extern int printf(const char *__restrict __format, ...);

#define LOG_PRINT(prefix, fmt, ...)                                                                \
    do                                                                                             \
    {                                                                                              \
        critical_state_t state = critical_enter();                                                 \
        printf(prefix fmt, ##__VA_ARGS__);                                                         \
        critical_exit(state);                                                                      \
    } while (0)

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) LOG_PRINT("[D] ", fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) LOG_PRINT("[I] ", fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...) LOG_PRINT("[W] ", fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) LOG_PRINT("[E] ", fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) ((void)0)
#endif
