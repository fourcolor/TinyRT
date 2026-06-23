#include <stdint.h>

#include "error.h"
#include "handle.h"
#include "logger.h"
#include "msg_queue.h"
#include "mutex.h"
#include "semaphore.h"
#include "task.h"
#include "test.h"
#include "timer.h"

typedef struct
{
    uint32_t value;
} destroy_msg_t;

static trt_handle_t sem_destroy_test;
static trt_handle_t mutex_destroy_test;
static trt_handle_t msg_read_q;
static trt_handle_t msg_write_q;
static trt_handle_t timer_destroy_test;

static volatile int sem_waiter_done;
static volatile int sem_timeout_waiter_done;
static volatile int mutex_waiter_done;
static volatile int mutex_holder_done;
static volatile int msg_reader_done;
static volatile int msg_writer_done;
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
    result = trt_sem_wait(sem_destroy_test);
    test_check_result("sem waiter woke destroyed", result == ERR_DESTROYED, result);
    sem_waiter_done = 1;

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void sem_timeout_waiter_task(void *arg)
{
    err_t result;

    (void)arg;
    result = trt_sem_wait_timeout(sem_destroy_test, TRT_SEC(10));
    test_check_result("sem timeout waiter woke destroyed", result == ERR_DESTROYED, result);
    sem_timeout_waiter_done = 1;

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void sem_destroyer_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(200));

    result = trt_sem_destroy(sem_destroy_test);
    test_check_result("sem destroy", result == ERR_OK, result);
    result = trt_sem_post(sem_destroy_test);
    test_check_result("sem old handle invalid", result == ERR_INVAL, result);

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void mutex_holder_task(void *arg)
{
    err_t result;

    (void)arg;
    result = trt_mutex_lock(mutex_destroy_test);
    test_check_result("mutex holder lock", result == ERR_OK, result);

    task_sleep(TRT_MS(500));
    result = trt_mutex_unlock(mutex_destroy_test);
    test_check_result("mutex holder old handle invalid", result == ERR_INVAL, result);
    mutex_holder_done = 1;

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void mutex_waiter_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(50));

    result = trt_mutex_lock(mutex_destroy_test);
    test_check_result("mutex waiter woke destroyed", result == ERR_DESTROYED, result);
    mutex_waiter_done = 1;

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void mutex_destroyer_task(void *arg)
{
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(250));

    result = trt_mutex_destroy(mutex_destroy_test);
    test_check_result("mutex destroy", result == ERR_OK, result);
    result = trt_mutex_trylock(mutex_destroy_test);
    test_check_result("mutex old handle invalid", result == ERR_INVAL, result);

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void msg_reader_task(void *arg)
{
    destroy_msg_t msg;
    err_t result;

    (void)arg;
    result = trt_msg_q_recv(msg_read_q, &msg, TRT_WAIT_FOREVER);
    test_check_result("msg reader woke destroyed", result == ERR_DESTROYED, result);
    msg_reader_done = 1;

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void msg_writer_task(void *arg)
{
    destroy_msg_t msg = {.value = 2};
    err_t result;

    (void)arg;
    result = trt_msg_q_send(msg_write_q, &msg, sizeof(msg), TRT_WAIT_FOREVER);
    test_check_result("msg writer woke destroyed", result == ERR_DESTROYED, result);
    msg_writer_done = 1;

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void msg_destroyer_task(void *arg)
{
    destroy_msg_t msg = {.value = 3};
    err_t result;

    (void)arg;
    task_sleep(TRT_MS(300));

    result = trt_msg_q_destroy(msg_read_q);
    test_check_result("msg read destroy", result == ERR_OK, result);
    result = trt_msg_q_recv(msg_read_q, &msg, TRT_US(0));
    test_check_result("msg read old handle invalid", result == ERR_INVAL, result);

    result = trt_msg_q_destroy(msg_write_q);
    test_check_result("msg write destroy", result == ERR_OK, result);
    result = trt_msg_q_send(msg_write_q, &msg, sizeof(msg), TRT_US(0));
    test_check_result("msg write old handle invalid", result == ERR_INVAL, result);

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void timer_destroyer_task(void *arg)
{
    err_t result;

    (void)arg;
    timer_destroy_test = trt_timer_create(destroy_timer_callback, 0);
    trt_timer_start(timer_destroy_test, TRT_MS(500), TRT_US(0));
    task_sleep(TRT_MS(100));

    result = trt_timer_destroy(timer_destroy_test);
    test_check_result("timer destroy", result == ERR_OK, result);
    task_sleep(TRT_MS(600));
    test_check("timer did not fire after destroy", timer_hits == 0);
    test_check("timer old handle inactive", trt_timer_active(timer_destroy_test) == 0);
    test_check("timer old handle invalid", trt_handle_valid(timer_destroy_test) == 0);

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

static void summary_task(void *arg)
{
    (void)arg;

    task_sleep(TRT_MS(1000));
    test_check("sem waiters completed", sem_waiter_done && sem_timeout_waiter_done);
    test_check("mutex tasks completed", mutex_waiter_done && mutex_holder_done);
    test_check("msg tasks completed", msg_reader_done && msg_writer_done);
    test_check("sem handle invalid", trt_handle_valid(sem_destroy_test) == 0);
    test_check("mutex handle invalid", trt_handle_valid(mutex_destroy_test) == 0);
    test_check("msg read handle invalid", trt_handle_valid(msg_read_q) == 0);
    test_check("msg write handle invalid", trt_handle_valid(msg_write_q) == 0);
    test_check("timer handle invalid", trt_handle_valid(timer_destroy_test) == 0);
    test_summary();

    for (;;)
    {
        task_sleep(TRT_SEC(1));
    }
}

void app_main(void)
{
    destroy_msg_t prefill = {.value = 1};

    test_begin("destroy");

    sem_destroy_test = trt_sem_create(1, 0);
    mutex_destroy_test = trt_mutex_create();
    msg_read_q = trt_msg_q_create(sizeof(destroy_msg_t), 1);
    msg_write_q = trt_msg_q_create(sizeof(destroy_msg_t), 1);
    trt_msg_q_send(msg_write_q, &prefill, sizeof(prefill), TRT_US(0));

    test_check("sem created", sem_destroy_test != TRT_HANDLE_INVALID);
    test_check("mutex created", mutex_destroy_test != TRT_HANDLE_INVALID);
    test_check("msg read queue created", msg_read_q != TRT_HANDLE_INVALID);
    test_check("msg write queue created", msg_write_q != TRT_HANDLE_INVALID);

    task_create("sem_waiter", sem_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_timeout", sem_timeout_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("sem_destroy", sem_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_holder", mutex_holder_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_waiter", mutex_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("mutex_destroy", mutex_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("msg_reader", msg_reader_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("msg_writer", msg_writer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("msg_destroy", msg_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("timer_destroy", timer_destroyer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("destroy_summary", summary_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
