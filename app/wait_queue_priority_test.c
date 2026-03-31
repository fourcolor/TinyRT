#include "logger.h"
#include "rtos_config.h"
#include "semaphore.h"
#include "task.h"
#include "timer.h"

static trt_sem_t priority_sem;
static volatile uint32_t wake_order;

static void high_waiter(void *arg)
{
    (void)arg;

    LOG_INFO("high waiter start\n");
    trt_sem_wait(&priority_sem);
    LOG_INFO("high waiter woke order=%lu tick=%lu\n", ++wake_order, timer_ticks());

    for (;;)
    {
        task_delay(1000);
    }
}

static void low_waiter(void *arg)
{
    (void)arg;

    LOG_INFO("low waiter start\n");
    trt_sem_wait(&priority_sem);
    LOG_INFO("low waiter woke order=%lu tick=%lu\n", ++wake_order, timer_ticks());

    for (;;)
    {
        task_delay(1000);
    }
}

static void poster(void *arg)
{
    (void)arg;

    LOG_INFO("priority poster start\n");
    task_delay(100);
    trt_sem_post(&priority_sem);
    LOG_INFO("priority post 1 tick=%lu\n", timer_ticks());

    task_delay(100);
    trt_sem_post(&priority_sem);
    LOG_INFO("priority post 2 tick=%lu\n", timer_ticks());

    for (;;)
    {
        task_delay(1000);
    }
}

void app_main(void)
{
    trt_sem_init(&priority_sem, 2, 0);

    task_create("high_waiter", high_waiter, 0, RTOS_TASK_STACK_SIZE, 3);
    task_create("low_waiter", low_waiter, 0, RTOS_TASK_STACK_SIZE, 2);
    task_create("poster", poster, 0, RTOS_TASK_STACK_SIZE, 1);
}
