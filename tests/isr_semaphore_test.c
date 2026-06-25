#include <stdint.h>

#include "error.h"
#include "logger.h"
#include "semaphore.h"
#include "task.h"
#include "timer.h"

static trt_handle_t sem_isr_signal;
static volatile uint32_t sem_isr_posts;
static volatile uint32_t sem_isr_wakes;

static void sem_isr_callback(void *arg)
{
    UNUSED(arg);

    if (trt_sem_post_from_isr(sem_isr_signal) == ERR_OK)
    {
        sem_isr_posts++;
    }
}

static void sem_isr_waiter_task(void *arg)
{
    static trt_handle_t sem_isr_timer;
    err_t task_context_result;

    UNUSED(arg);
    LOG_INFO("sem isr waiter start\n");

    task_context_result = trt_sem_post_from_isr(sem_isr_signal);
    LOG_INFO("sem post from task result=%d tick=%lu\n", task_context_result, timer_ticks());

    sem_isr_timer = trt_timer_create(sem_isr_callback, 0);
    trt_timer_start(sem_isr_timer, TRT_MS(750), TRT_MS(750));

    for (;;)
    {
        if (trt_sem_wait(sem_isr_signal) == ERR_OK)
        {
            sem_isr_wakes++;
            LOG_INFO("SEM ISR wake post=%lu wake=%lu tick=%lu\n", sem_isr_posts, sem_isr_wakes,
                     timer_ticks());
        }
    }
}

void app_main(void)
{
    sem_isr_signal = trt_sem_create(1, 0);

    task_create("sem_isr_wait", sem_isr_waiter_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
