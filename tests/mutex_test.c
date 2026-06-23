#include <stdint.h>

#include "error.h"
#include "logger.h"
#include "mutex.h"
#include "sched.h"
#include "task.h"
#include "timer.h"

static trt_mutex_t mutex_test;
static trt_mutex_t mutex_recursive;

static void mutex_holder_task(void *arg)
{
    (void)arg;
    LOG_INFO("mutex holder start\n");

    if (trt_mutex_lock(&mutex_test) == ERR_OK)
    {
        LOG_INFO("mutex holder locked tick=%lu\n", timer_ticks());
        task_sleep(TRT_MS(1000));
        trt_mutex_unlock(&mutex_test);
        LOG_INFO("mutex holder unlocked tick=%lu\n", timer_ticks());
    }

    for (;;)
    {
        task_sleep(TRT_MS(5000));
    }
}

static void mutex_timeout_waiter_task(void *arg)
{
    uint32_t start;
    int result;

    (void)arg;
    LOG_INFO("mutex timeout waiter start\n");

    task_sleep(TRT_MS(20));
    start = timer_ticks();
    result = trt_mutex_lock_timeout(&mutex_test, TRT_MS(300));
    LOG_INFO("mutex timeout result=%d elapsed=%lu tick=%lu\n", result, timer_ticks() - start,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(5000));
    }
}

static void mutex_success_waiter_task(void *arg)
{
    uint32_t start;
    int result;

    (void)arg;
    LOG_INFO("mutex success waiter start\n");

    task_sleep(TRT_MS(40));
    start = timer_ticks();
    result = trt_mutex_lock_timeout(&mutex_test, TRT_SEC(2));
    LOG_INFO("mutex success result=%d elapsed=%lu tick=%lu\n", result, timer_ticks() - start,
             timer_ticks());
    if (result == ERR_OK)
    {
        trt_mutex_unlock(&mutex_test);
        LOG_INFO("mutex success unlocked tick=%lu\n", timer_ticks());
    }

    for (;;)
    {
        task_sleep(TRT_MS(5000));
    }
}

static void mutex_misc_task(void *arg)
{
    int lock_result;
    int first;
    int second;

    (void)arg;
    LOG_INFO("mutex misc start\n");

    task_sleep(TRT_MS(10));
    sched_lock();
    lock_result = trt_mutex_lock_timeout(&mutex_test, TRT_MS(100));
    sched_unlock();
    LOG_INFO("mutex sched lock result=%d tick=%lu\n", lock_result, timer_ticks());

    first = trt_mutex_lock(&mutex_recursive);
    second = trt_mutex_lock(&mutex_recursive);
    trt_mutex_unlock(&mutex_recursive);
    trt_mutex_unlock(&mutex_recursive);
    LOG_INFO("mutex recursive first=%d second=%d tick=%lu\n", first, second, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(5000));
    }
}

void app_main(void)
{
    trt_mutex_init(&mutex_test);
    trt_mutex_init(&mutex_recursive);

    task_create("mutex_holder", mutex_holder_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_to_wait", mutex_timeout_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_ok_wait", mutex_success_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_misc", mutex_misc_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
