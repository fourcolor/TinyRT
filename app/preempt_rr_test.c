#include <stdint.h>
#include "logger.h"
#include "rtos_config.h"
#include "task.h"
#include "timer.h"

static volatile uint32_t rr_a_count;
static volatile uint32_t rr_b_count;

static void rr_busy_a(void *arg)
{
    (void)arg;

    LOG_INFO("rr busy A start\n");
    for (;;)
    {
        rr_a_count++;
    }
}

static void rr_busy_b(void *arg)
{
    (void)arg;

    LOG_INFO("rr busy B start\n");
    for (;;)
    {
        rr_b_count++;
    }
}

static void rr_monitor(void *arg)
{
    uint32_t prev_a = 0;
    uint32_t prev_b = 0;

    (void)arg;
    LOG_INFO("rr monitor start preempt=%d round_robin=%d slice=%d\n", RTOS_SCHED_PREEMPTIVE,
             RTOS_SCHED_ROUND_ROBIN, RTOS_TIME_SLICE_TICKS);

    for (;;)
    {
        uint32_t now_a;
        uint32_t now_b;
        uint32_t delta_a;
        uint32_t delta_b;

        task_delay(1000);

        now_a = rr_a_count;
        now_b = rr_b_count;
        delta_a = now_a - prev_a;
        delta_b = now_b - prev_b;
        prev_a = now_a;
        prev_b = now_b;

        LOG_INFO("RR tick=%lu a=%lu b=%lu da=%lu db=%lu\n", timer_ticks(), now_a, now_b, delta_a,
                 delta_b);

        if (delta_a == 0 || delta_b == 0)
        {
            LOG_ERROR("RR failed: one busy task did not run\n");
        }
    }
}

void app_main(void)
{
    task_create("rr_busy_a", rr_busy_a, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("rr_busy_b", rr_busy_b, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("rr_monitor", rr_monitor, 0, RTOS_TASK_STACK_SIZE, 2);
}
