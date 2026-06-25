#include "kernel.h"
#include "logger.h"
#include "task.h"
#include "timer.h"

static volatile uint32_t worker_a_count;
static volatile uint32_t worker_b_count;
static volatile uint32_t worker_c_count;

static void delay_worker(void *arg)
{
    volatile uint32_t *counter = arg;

    for (;;)
    {
        (*counter)++;
        task_sleep(TRT_MS(1));
    }
}

static void monitor_task(void *arg)
{
    uint32_t last_a = 0;
    uint32_t last_b = 0;
    uint32_t last_c = 0;

    UNUSED(arg);

    for (;;)
    {
        task_sleep(TRT_MS(1000));
        LOG_INFO("delay race tick=%lu a=%lu b=%lu c=%lu\n", timer_ticks(), worker_a_count,
                 worker_b_count, worker_c_count);

        if (worker_a_count == last_a || worker_b_count == last_b || worker_c_count == last_c)
        {
            LOG_ERROR("delay race stalled tick=%lu a=%lu b=%lu c=%lu\n", timer_ticks(),
                      worker_a_count, worker_b_count, worker_c_count);
            for (;;)
            {
            }
        }

        last_a = worker_a_count;
        last_b = worker_b_count;
        last_c = worker_c_count;
    }
}

void app_main(void)
{
    task_create("delay_a", delay_worker, (void *)&worker_a_count, RTOS_TASK_STACK_SIZE, 5);
    task_create("delay_b", delay_worker, (void *)&worker_b_count, RTOS_TASK_STACK_SIZE, 5);
    task_create("delay_c", delay_worker, (void *)&worker_c_count, RTOS_TASK_STACK_SIZE, 5);
    task_create("delay_mon", monitor_task, 0, RTOS_TASK_STACK_SIZE, 6);
}
