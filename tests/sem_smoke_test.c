#include "error.h"
#include "logger.h"
#include "rtos_config.h"
#include "semaphore.h"
#include "task.h"
#include "timer.h"

static trt_handle_t sem;
static volatile uint32_t posts;
static volatile uint32_t wakes;

static void producer_task(void *arg)
{
    (void)arg;
    LOG_INFO("sem smoke producer start\n");

    for (;;)
    {
        task_sleep(TRT_MS(500));
        posts++;
        trt_sem_post(sem);
        LOG_INFO("sem smoke post=%lu wake=%lu tick=%lu\n", posts, wakes, timer_ticks());
    }
}

static void consumer_task(void *arg)
{
    (void)arg;
    LOG_INFO("sem smoke consumer start\n");

    for (;;)
    {
        err_t result = trt_sem_wait(sem);

        if (result != ERR_OK)
        {
            LOG_INFO("sem smoke wait failed result=%d tick=%lu\n", result, timer_ticks());
            task_sleep(TRT_MS(100));
            continue;
        }

        wakes++;
        LOG_INFO("sem smoke woke post=%lu wake=%lu tick=%lu\n", posts, wakes, timer_ticks());
    }
}

void app_main(void)
{
    sem = trt_sem_create(1, 0);
    task_create("sem_prod", producer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_cons", consumer_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
