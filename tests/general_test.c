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
    uint32_t seq;
    uint32_t tick;
} general_msg_t;

static trt_handle_t general_sem;
static trt_handle_t general_mutex;
static trt_handle_t general_q;
static trt_handle_t general_timer;

static volatile uint32_t timer_events;
static volatile uint32_t sem_posts;
static volatile uint32_t sem_wakes;
static volatile uint32_t mutex_entries;
static volatile uint32_t msg_sent;
static volatile uint32_t msg_received;
static volatile uint32_t errors;

static void general_timer_callback(void *arg)
{
    (void)arg;
    timer_events++;
    trt_sem_post_from_isr(general_sem);
}

static void sem_consumer_task(void *arg)
{
    (void)arg;
    LOG_INFO("general sem consumer start\n");

    for (;;)
    {
        err_t result = trt_sem_wait(general_sem);

        if (result == ERR_OK)
        {
            sem_wakes++;
        }
        else
        {
            errors++;
            LOG_INFO("general sem wait error=%d tick=%lu\n", result, timer_ticks());
            task_sleep(TRT_MS(100));
        }
    }
}

static void timer_task(void *arg)
{
    (void)arg;
    LOG_INFO("general timer task start\n");

    general_timer = trt_timer_create(general_timer_callback, 0);
    trt_timer_start(general_timer, TRT_MS(250), TRT_MS(250));

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

static void sem_producer_task(void *arg)
{
    (void)arg;
    LOG_INFO("general sem producer start\n");

    for (;;)
    {
        task_sleep(TRT_MS(350));
        if (trt_sem_post(general_sem) == ERR_OK)
        {
            sem_posts++;
        }
        else
        {
            errors++;
        }
    }
}

static void mutex_task(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;

    LOG_INFO("general mutex task%lu start\n", id);

    for (;;)
    {
        err_t result = trt_mutex_lock(general_mutex);

        if (result == ERR_OK)
        {
            mutex_entries++;
            task_sleep(TRT_MS(20));
            trt_mutex_unlock(general_mutex);
        }
        else
        {
            errors++;
            LOG_INFO("general mutex lock error=%d id=%lu tick=%lu\n", result, id, timer_ticks());
        }

        task_sleep(TRT_MS(120 + (id * 30)));
    }
}

static void msg_producer_task(void *arg)
{
    uint32_t seq = 0;

    (void)arg;
    LOG_INFO("general msg producer start\n");

    for (;;)
    {
        general_msg_t msg;
        err_t result;

        msg.seq = seq;
        msg.tick = timer_ticks();
        result = trt_msg_q_send(general_q, &msg, sizeof(msg), TRT_MS(500));
        if (result == ERR_OK)
        {
            seq++;
            msg_sent++;
        }
        else
        {
            errors++;
            LOG_INFO("general msg send error=%d seq=%lu tick=%lu\n", result, seq, timer_ticks());
        }

        task_sleep(TRT_MS(80));
    }
}

static void msg_consumer_task(void *arg)
{
    uint32_t expected = 0;

    (void)arg;
    LOG_INFO("general msg consumer start\n");

    for (;;)
    {
        general_msg_t msg;
        err_t result;

        result = trt_msg_q_recv(general_q, &msg, TRT_MS(1000));
        if (result == ERR_OK)
        {
            if (msg.seq != expected)
            {
                errors++;
                LOG_INFO("general msg mismatch got=%lu expect=%lu tick=%lu\n", msg.seq, expected,
                         timer_ticks());
                expected = msg.seq + 1u;
            }
            else
            {
                expected++;
            }
            msg_received++;
        }
        else
        {
            errors++;
            LOG_INFO("general msg recv error=%d tick=%lu\n", result, timer_ticks());
        }

        task_sleep(TRT_MS(110));
    }
}

static void monitor_task(void *arg)
{
    (void)arg;
    LOG_INFO("general monitor start\n");

    for (;;)
    {
        LOG_INFO(
            "GENERAL tick=%lu timer=%lu sem_post=%lu sem_wake=%lu mutex=%lu msg=%lu/%lu err=%lu\n",
            timer_ticks(), timer_events, sem_posts, sem_wakes, mutex_entries, msg_sent,
            msg_received, errors);
        task_sleep(TRT_MS(1000));
    }
}

void app_main(void)
{
    general_sem = trt_sem_create(8, 0);
    general_mutex = trt_mutex_create();

    general_q = trt_msg_q_create(sizeof(general_msg_t), 8);
    if (general_sem == TRT_HANDLE_INVALID || general_mutex == TRT_HANDLE_INVALID ||
        general_q == TRT_HANDLE_INVALID)
    {
        LOG_ERROR("general object init failed sem=%lu mutex=%lu queue=%lu\n", general_sem,
                  general_mutex, general_q);
        return;
    }

    task_create("gen_timer", timer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_sem_cons", sem_consumer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_sem_prod", sem_producer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_mutex_a", mutex_task, (void *)1, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_mutex_b", mutex_task, (void *)2, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_msg_prod", msg_producer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_msg_cons", msg_consumer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("gen_monitor", monitor_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
