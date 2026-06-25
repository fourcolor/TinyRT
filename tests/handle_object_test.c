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
} handle_msg_t;

static trt_handle_t sem_handle;
static trt_handle_t mutex_handle;
static trt_handle_t queue_handle;
static trt_handle_t timer_handle;
static volatile uint32_t timer_posts;

static void timer_callback(void *arg)
{
    UNUSED(arg);

    if (trt_sem_post_from_isr(sem_handle) == ERR_OK)
    {
        timer_posts++;
    }
}

static void producer_task(void *arg)
{
    uint32_t seq = 0;

    UNUSED(arg);
    for (;;)
    {
        handle_msg_t msg = {
            .seq = seq++,
            .tick = timer_ticks(),
        };

        trt_mutex_lock(mutex_handle);
        trt_msg_q_send(queue_handle, &msg, sizeof(msg), TRT_MS(500));
        trt_mutex_unlock(mutex_handle);
        task_sleep(TRT_MS(200));
    }
}

static void consumer_task(void *arg)
{
    handle_msg_t msg;

    UNUSED(arg);
    for (;;)
    {
        if (trt_msg_q_recv(queue_handle, &msg, TRT_SEC(1)) == ERR_OK)
        {
            LOG_INFO("handle msg seq=%lu msg_tick=%lu now=%lu\n", msg.seq, msg.tick, timer_ticks());
        }
    }
}

static void sem_task(void *arg)
{
    UNUSED(arg);
    for (;;)
    {
        if (trt_sem_wait_timeout(sem_handle, TRT_SEC(2)) == ERR_OK)
        {
            LOG_INFO("handle sem wake posts=%lu tick=%lu\n", timer_posts, timer_ticks());
        }
    }
}

void app_main(void)
{
    sem_handle = trt_sem_create(8, 0);
    mutex_handle = trt_mutex_create();
    queue_handle = trt_msg_q_create(sizeof(handle_msg_t), 4);
    timer_handle = trt_timer_create(timer_callback, 0);

    LOG_INFO("handle test sem=%lu mutex=%lu queue=%lu timer=%lu\n", sem_handle, mutex_handle,
             queue_handle, timer_handle);

    trt_timer_start(timer_handle, TRT_MS(500), TRT_MS(500));
    task_create("handle_prod", producer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("handle_cons", consumer_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("handle_sem", sem_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
