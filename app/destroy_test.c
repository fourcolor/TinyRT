#include <stdint.h>

#include "error.h"
#include "logger.h"
#include "msg_queue.h"
#include "mutex.h"
#include "semaphore.h"
#include "task.h"
#include "timer.h"

typedef struct
{
    uint32_t value;
} destroy_msg_t;

static trt_sem_t sem_destroy_test;
static trt_mutex_t mutex_destroy_test;
static trt_msg_q_t *msg_read_q;
static trt_msg_q_t *msg_write_q;
static timer_t destroy_timer;
static volatile uint32_t timer_hits;

static void destroy_timer_callback(void *arg)
{
    (void)arg;
    timer_hits++;
}

static void sem_waiter_task(void *arg)
{
    err_t result;

    (void)arg;
    LOG_INFO("destroy sem waiter start\n");

    result = trt_sem_wait(&sem_destroy_test);
    LOG_INFO("destroy sem wait result=%d expect=%d tick=%lu\n", result, ERR_DESTROYED,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void sem_destroyer_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(200));

    result = trt_sem_destroy(&sem_destroy_test);
    LOG_INFO("destroy sem result=%d tick=%lu\n", result, timer_ticks());

    result = trt_sem_post(&sem_destroy_test);
    LOG_INFO("destroy sem post result=%d expect=%d tick=%lu\n", result, ERR_DESTROYED,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void mutex_holder_task(void *arg)
{
    err_t result;

    (void)arg;
    LOG_INFO("destroy mutex holder start\n");

    result = trt_mutex_lock(&mutex_destroy_test);
    LOG_INFO("destroy mutex holder lock result=%d tick=%lu\n", result, timer_ticks());
    task_sleep(TRT_MS(700));

    result = trt_mutex_unlock(&mutex_destroy_test);
    LOG_INFO("destroy mutex holder unlock result=%d expect=%d tick=%lu\n", result, ERR_DESTROYED,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void mutex_waiter_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(50));

    result = trt_mutex_lock(&mutex_destroy_test);
    LOG_INFO("destroy mutex wait result=%d expect=%d tick=%lu\n", result, ERR_DESTROYED,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void mutex_destroyer_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(300));

    result = trt_mutex_destroy(&mutex_destroy_test);
    LOG_INFO("destroy mutex result=%d tick=%lu\n", result, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void msg_reader_task(void *arg)
{
    destroy_msg_t msg;
    err_t result;

    (void)arg;
    LOG_INFO("destroy msg reader start\n");

    result = trt_msg_q_recv(msg_read_q, &msg, TRT_WAIT_FOREVER);
    LOG_INFO("destroy msg read result=%d expect=%d tick=%lu\n", result, ERR_DESTROYED,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void msg_writer_task(void *arg)
{
    destroy_msg_t msg = {.value = 2};
    err_t result;

    (void)arg;
    LOG_INFO("destroy msg writer start\n");

    result = trt_msg_q_send(msg_write_q, &msg, sizeof(msg), TRT_WAIT_FOREVER);
    LOG_INFO("destroy msg write result=%d expect=%d tick=%lu\n", result, ERR_DESTROYED,
             timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void msg_destroyer_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(400));

    result = trt_msg_q_destroy(msg_read_q);
    LOG_INFO("destroy msg read_q result=%d tick=%lu\n", result, timer_ticks());
    msg_read_q = 0;

    result = trt_msg_q_destroy(msg_write_q);
    LOG_INFO("destroy msg write_q result=%d tick=%lu\n", result, timer_ticks());
    msg_write_q = 0;

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void timer_destroy_task(void *arg)
{
    err_t result;

    (void)arg;
    LOG_INFO("destroy timer test start\n");

    timer_setup(&destroy_timer, destroy_timer_callback, 0);
    timer_start(&destroy_timer, TRT_MS(500), TRT_US(0));
    task_sleep(TRT_MS(100));

    result = timer_destroy(&destroy_timer);
    LOG_INFO("destroy timer result=%d tick=%lu\n", result, timer_ticks());

    task_sleep(TRT_MS(700));
    LOG_INFO("destroy timer hits=%lu expect=0 active=%d tick=%lu\n", timer_hits,
             timer_active(&destroy_timer), timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

void app_main(void)
{
    destroy_msg_t prefill = {.value = 1};

    trt_sem_init(&sem_destroy_test, 1, 0);
    trt_mutex_init(&mutex_destroy_test);

    msg_read_q = trt_msg_q_init(sizeof(destroy_msg_t), 1);
    msg_write_q = trt_msg_q_init(sizeof(destroy_msg_t), 1);
    if (msg_write_q != 0)
    {
        trt_msg_q_send(msg_write_q, &prefill, sizeof(prefill), TRT_US(0));
    }

    task_create("destroy_sem_wait", sem_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_sem", sem_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_mtx_hold", mutex_holder_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_mtx_wait", mutex_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_mtx", mutex_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_msg_read", msg_reader_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_msg_write", msg_writer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_msg", msg_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_timer", timer_destroy_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
