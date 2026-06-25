#include <stdint.h>

#include "error.h"
#include "logger.h"
#include "msg_queue.h"
#include "semaphore.h"
#include "task.h"
#include "timer.h"

typedef struct
{
    uint32_t value;
} delete_msg_t;

static trt_handle_t delete_sem;
static trt_handle_t read_q;
static trt_handle_t write_q;

static volatile uint32_t unexpected_wakes;
static volatile uint32_t errors;

static void sem_waiter_task(void *arg)
{
    err_t result;

    (void)arg;
    LOG_INFO("delete blocked sem waiter start tick=%lu\n", timer_ticks());

    result = trt_sem_wait(delete_sem);
    unexpected_wakes++;
    LOG_ERROR("delete blocked sem waiter woke result=%d tick=%lu\n", result, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void sem_timeout_waiter_task(void *arg)
{
    err_t result;

    (void)arg;
    LOG_INFO("delete blocked sem timeout waiter start tick=%lu\n", timer_ticks());

    result = trt_sem_wait_timeout(delete_sem, TRT_SEC(30));
    unexpected_wakes++;
    LOG_ERROR("delete blocked sem timeout waiter woke result=%d tick=%lu\n", result, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void msg_reader_task(void *arg)
{
    delete_msg_t msg;
    err_t result;

    (void)arg;
    LOG_INFO("delete blocked msg reader start tick=%lu\n", timer_ticks());

    result = trt_msg_q_recv(read_q, &msg, TRT_WAIT_FOREVER);
    unexpected_wakes++;
    LOG_ERROR("delete blocked msg reader woke result=%d tick=%lu\n", result, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void msg_writer_task(void *arg)
{
    delete_msg_t msg = {.value = 2};
    err_t result;

    (void)arg;
    LOG_INFO("delete blocked msg writer start tick=%lu\n", timer_ticks());

    result = trt_msg_q_send(write_q, &msg, sizeof(msg), TRT_WAIT_FOREVER);
    unexpected_wakes++;
    LOG_ERROR("delete blocked msg writer woke result=%d tick=%lu\n", result, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void delay_task(void *arg)
{
    (void)arg;
    LOG_INFO("delete blocked delay task start tick=%lu\n", timer_ticks());

    task_sleep(TRT_SEC(30));
    unexpected_wakes++;
    LOG_ERROR("delete blocked delay task woke tick=%lu\n", timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void check_result(const char *name, int ok)
{
    if (ok)
    {
        LOG_INFO("delete blocked %s ok tick=%lu\n", name, timer_ticks());
    }
    else
    {
        errors++;
        LOG_ERROR("delete blocked %s fail tick=%lu\n", name, timer_ticks());
    }
}

static void supervisor_task(void *arg)
{
    delete_msg_t prefill = {.value = 1};
    trt_handle_t sem_waiter;
    trt_handle_t sem_timeout_waiter;
    trt_handle_t msg_reader;
    trt_handle_t msg_writer;
    trt_handle_t delay_blocked;
    err_t result;

    (void)arg;
    LOG_INFO("delete blocked supervisor start tick=%lu\n", timer_ticks());

    read_q = trt_msg_q_create(sizeof(delete_msg_t), 1);
    write_q = trt_msg_q_create(sizeof(delete_msg_t), 1);
    if (read_q == TRT_HANDLE_INVALID || write_q == TRT_HANDLE_INVALID)
    {
        LOG_ERROR("delete blocked msg queue init failed\n");
        errors++;
        return;
    }

    result = trt_msg_q_send(write_q, &prefill, sizeof(prefill), TRT_US(0));
    check_result("prefill writer queue", result == ERR_OK);

    sem_waiter = task_create("del_sem_wait", sem_waiter_task, 0, RTOS_TASK_STACK_SIZE, 2);
    sem_timeout_waiter =
        task_create("del_sem_timeout", sem_timeout_waiter_task, 0, RTOS_TASK_STACK_SIZE, 2);
    msg_reader = task_create("del_msg_read", msg_reader_task, 0, RTOS_TASK_STACK_SIZE, 2);
    msg_writer = task_create("del_msg_write", msg_writer_task, 0, RTOS_TASK_STACK_SIZE, 2);
    delay_blocked = task_create("del_delay", delay_task, 0, RTOS_TASK_STACK_SIZE, 2);
    check_result("task create", sem_waiter != TRT_HANDLE_INVALID &&
                                    sem_timeout_waiter != TRT_HANDLE_INVALID &&
                                    msg_reader != TRT_HANDLE_INVALID &&
                                    msg_writer != TRT_HANDLE_INVALID &&
                                    delay_blocked != TRT_HANDLE_INVALID);

    task_sleep(TRT_MS(200));

    check_result("tasks reached blocking window", unexpected_wakes == 0);

    check_result("delete sem waiter", task_delete(sem_waiter) == ERR_OK);
    check_result("delete sem timeout waiter", task_delete(sem_timeout_waiter) == ERR_OK);
    check_result("delete msg reader", task_delete(msg_reader) == ERR_OK);
    check_result("delete msg writer", task_delete(msg_writer) == ERR_OK);
    check_result("delete delay task", task_delete(delay_blocked) == ERR_OK);

    task_sleep(TRT_MS(100));

    result = trt_sem_post(delete_sem);
    check_result("sem usable after deletes", result == ERR_OK);

    result = trt_msg_q_destroy(read_q);
    check_result("destroy read queue", result == ERR_OK);

    result = trt_msg_q_destroy(write_q);
    check_result("destroy write queue", result == ERR_OK);

    task_sleep(TRT_MS(1200));
    check_result("deleted tasks stayed deleted", unexpected_wakes == 0);

    LOG_INFO("DELETE_BLOCKED summary errors=%lu unexpected_wakes=%lu tick=%lu\n", errors,
             unexpected_wakes, timer_ticks());

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

void app_main(void)
{
    delete_sem = trt_sem_create(1, 0);

    task_create("del_block_super", supervisor_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
