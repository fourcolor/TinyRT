#include <stdint.h>

#include "app_tests.h"
#include "error.h"
#include "logger.h"
#include "mutex.h"
#include "sched.h"
#include "semaphore.h"
#include "task.h"
#include "timer.h"

static trt_sem_t sem_test;
static trt_sem_t sem_lock_test;
static trt_sem_t sem_timeout_only;
static trt_sem_t sem_timeout_signal;
static trt_mutex_t mutex_test;
static trt_mutex_t mutex_recursive;
static volatile uint32_t sem_posts;
static volatile uint32_t sem_wakes;

static void sem_producer_task(void *arg)
{
    (void)arg;
    LOG_INFO("sem producer start\n");

    for (;;)
    {
        task_delay(1500);
        sem_posts++;
        trt_sem_post(&sem_test);
        LOG_INFO("SEM post=%lu wake=%lu tick=%lu\n", sem_posts, sem_wakes, timer_ticks());
    }
}

static void sem_consumer_task(void *arg)
{
    (void)arg;
    LOG_INFO("sem consumer start\n");

    for (;;)
    {
        if (trt_sem_wait(&sem_test) != ERR_OK)
        {
            LOG_INFO("SEM wait failed tick=%lu\n", timer_ticks());
            task_delay(1000);
            continue;
        }

        sem_wakes++;
        LOG_INFO("SEM wait woke post=%lu wake=%lu tick=%lu\n", sem_posts, sem_wakes, timer_ticks());
    }
}

static void sem_lock_test_task(void *arg)
{
    int result;
    int timeout_result;

    (void)arg;
    LOG_INFO("sem lock test start\n");

    sched_lock();
    result = trt_sem_wait(&sem_lock_test);
    timeout_result = trt_sem_wait_timeout(&sem_lock_test, TRT_MS(100));
    sched_unlock();

    LOG_INFO("sem lock wait result=%d timeout_result=%d tick=%lu\n", result, timeout_result,
             timer_ticks());

    for (;;)
    {
        task_delay(5000);
    }
}

static void sem_timeout_task(void *arg)
{
    uint32_t start;
    int result;

    (void)arg;
    LOG_INFO("sem timeout task start\n");

    start = timer_ticks();
    result = trt_sem_wait_timeout(&sem_timeout_only, TRT_MS(500));
    LOG_INFO("sem timeout result=%d elapsed=%lu tick=%lu\n", result, timer_ticks() - start,
             timer_ticks());

    for (;;)
    {
        task_delay(5000);
    }
}

static void sem_timeout_signal_waiter_task(void *arg)
{
    uint32_t start;
    int result;

    (void)arg;
    LOG_INFO("sem timeout signal waiter start\n");

    start = timer_ticks();
    result = trt_sem_wait_timeout(&sem_timeout_signal, TRT_MS(1000));
    LOG_INFO("sem timeout signal result=%d elapsed=%lu tick=%lu\n", result, timer_ticks() - start,
             timer_ticks());

    for (;;)
    {
        task_delay(5000);
    }
}

static void sem_timeout_signal_poster_task(void *arg)
{
    (void)arg;
    LOG_INFO("sem timeout signal poster start\n");

    task_delay(300);
    trt_sem_post(&sem_timeout_signal);
    LOG_INFO("sem timeout signal posted tick=%lu\n", timer_ticks());

    for (;;)
    {
        task_delay(5000);
    }
}

static void mutex_holder_task(void *arg)
{
    (void)arg;
    LOG_INFO("mutex holder start\n");

    if (trt_mutex_lock(&mutex_test) == ERR_OK)
    {
        LOG_INFO("mutex holder locked tick=%lu\n", timer_ticks());
        task_delay(1000);
        trt_mutex_unlock(&mutex_test);
        LOG_INFO("mutex holder unlocked tick=%lu\n", timer_ticks());
    }

    for (;;)
    {
        task_delay(5000);
    }
}

static void mutex_timeout_waiter_task(void *arg)
{
    uint32_t start;
    int result;

    (void)arg;
    LOG_INFO("mutex timeout waiter start\n");

    task_delay(20);
    start = timer_ticks();
    result = trt_mutex_lock_timeout(&mutex_test, TRT_MS(300));
    LOG_INFO("mutex timeout result=%d elapsed=%lu tick=%lu\n", result, timer_ticks() - start,
             timer_ticks());

    for (;;)
    {
        task_delay(5000);
    }
}

static void mutex_success_waiter_task(void *arg)
{
    uint32_t start;
    int result;

    (void)arg;
    LOG_INFO("mutex success waiter start\n");

    task_delay(40);
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
        task_delay(5000);
    }
}

static void mutex_misc_task(void *arg)
{
    int lock_result;
    int first;
    int second;

    (void)arg;
    LOG_INFO("mutex misc start\n");

    task_delay(10);
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
        task_delay(5000);
    }
}

void app_test_sync_start(void)
{
    trt_sem_init(&sem_test, 1, 0);
    trt_sem_init(&sem_lock_test, 1, 0);
    trt_sem_init(&sem_timeout_only, 1, 0);
    trt_sem_init(&sem_timeout_signal, 1, 0);
    trt_mutex_init(&mutex_test);
    trt_mutex_init(&mutex_recursive);

    task_create("sem_prod", sem_producer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_cons", sem_consumer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_lock_test", sem_lock_test_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_timeout", sem_timeout_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_to_waiter", sem_timeout_signal_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_to_poster", sem_timeout_signal_poster_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_holder", mutex_holder_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_to_wait", mutex_timeout_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_ok_wait", mutex_success_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_misc", mutex_misc_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
