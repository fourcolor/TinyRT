#pragma once

#include <stdint.h>

#include "critical.h"
#include "error.h"
#include "logger.h"
#include "timer.h"

static const char *test_current_name;
static uint32_t test_total_count;
static uint32_t test_fail_count;

static inline void test_begin(const char *name)
{
    critical_state_t state;

    state = critical_enter();
    test_current_name = name;
    test_total_count = 0;
    test_fail_count = 0;
    critical_exit(state);

    LOG_INFO("TEST %s start tick=%lu\n", test_current_name, timer_ticks());
}

static inline void test_check(const char *name, int ok)
{
    critical_state_t state;

    state = critical_enter();
    test_total_count++;
    if (!ok)
    {
        test_fail_count++;
    }
    critical_exit(state);

    LOG_INFO("TEST %s %s %s tick=%lu\n", test_current_name, name, ok ? "ok" : "fail",
             timer_ticks());
}

static inline void test_check_result(const char *name, int ok, err_t result)
{
    critical_state_t state;

    state = critical_enter();
    test_total_count++;
    if (!ok)
    {
        test_fail_count++;
    }
    critical_exit(state);

    LOG_INFO("TEST %s %s %s result=%d tick=%lu\n", test_current_name, name,
             ok ? "ok" : "fail", result, timer_ticks());
}

static inline uint32_t test_failures(void)
{
    uint32_t failures;
    critical_state_t state;

    state = critical_enter();
    failures = test_fail_count;
    critical_exit(state);

    return failures;
}

static inline void test_summary(void)
{
    uint32_t total;
    uint32_t failed;
    critical_state_t state;

    state = critical_enter();
    total = test_total_count;
    failed = test_fail_count;
    critical_exit(state);

    LOG_INFO("TEST %s summary total=%lu failed=%lu tick=%lu\n", test_current_name, total, failed,
             timer_ticks());
}
