#include "wait_queue.h"
#include "critical.h"
#include "error.h"
#include "hal.h"
#include "sched.h"
#include "task.h"
#include "timer.h"

void trt_wait_q_init(trt_wait_q_t *queue)
{
    INIT_LIST_HEAD(&queue->waiters);
}

int trt_wait_q_empty(trt_wait_q_t *queue)
{
    return list_empty(&queue->waiters);
}

err_t trt_wait_q_block_locked(trt_wait_q_t *queue)
{
    if (queue == 0 || scheduler.current_task == 0 || sched_is_locked())
    {
        sched_set_pending();
        return queue == 0 || scheduler.current_task == 0 ? ERR_INVAL : ERR_LOCKED;
    }

    scheduler.current_task->wait_result = TASK_WAIT_NONE;
    sched_block_current(&queue->waiters);
    return ERR_OK;
}

err_t trt_wait_q_block(trt_wait_q_t *queue)
{
    int result;
    critical_state_t state;

    state = critical_enter();
    result = trt_wait_q_block_locked(queue);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    return scheduler.current_task->wait_result == TASK_WAIT_OBJECT ? ERR_OK : ERR_STATE;
}

static err_t trt_wait_q_block_ticks_locked(trt_wait_q_t *queue, uint32_t timeout_ticks)
{
    int result;

    if (timeout_ticks == 0)
    {
        return ERR_TIMEOUT;
    }

    result = trt_wait_q_block_locked(queue);
    if (result != 0)
    {
        return result;
    }

    scheduler.current_task->wakeup = timer_ticks() + timeout_ticks;
    scheduler.current_task->waiting_on_timer = 1;
    scheduler.current_task->wait_result = TASK_WAIT_NONE;
    sched_timeout_insert(scheduler.current_task);

    return ERR_OK;
}

static err_t trt_wait_q_block_ticks(trt_wait_q_t *queue, uint32_t timeout_ticks)
{
    int result;
    critical_state_t state;

    state = critical_enter();
    result = trt_wait_q_block_ticks_locked(queue, timeout_ticks);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    return scheduler.current_task->wait_result == TASK_WAIT_OBJECT ? ERR_OK : ERR_TIMEOUT;
}

err_t trt_wait_q_block_timeout_locked(trt_wait_q_t *queue, trt_time_t timeout)
{
    return trt_wait_q_block_ticks_locked(queue, timer_us_to_ticks(timeout.us));
}

err_t trt_wait_q_block_timeout(trt_wait_q_t *queue, trt_time_t timeout)
{
    return trt_wait_q_block_ticks(queue, timer_us_to_ticks(timeout.us));
}

task_t *trt_wait_q_wake_one_locked(trt_wait_q_t *queue)
{
    task_t *task;

    if (queue == 0)
    {
        return 0;
    }

    task = sched_wake_one(&queue->waiters);
    if (task != 0 && sched_is_locked())
    {
        sched_set_pending();
    }
    if (task != 0)
    {
        task->wait_result = TASK_WAIT_OBJECT;
    }

    return task;
}

task_t *trt_wait_q_wake_one(trt_wait_q_t *queue)
{
    task_t *task;
    critical_state_t state;

    state = critical_enter();
    task = trt_wait_q_wake_one_locked(queue);
    critical_exit(state);

    return task;
}

task_t *trt_wait_q_wake_one_from_isr(trt_wait_q_t *queue)
{
    task_t *task;

    if (!arch_in_isr())
    {
        return 0;
    }

    task = trt_wait_q_wake_one_locked(queue);
    if (task != 0)
    {
        sched_request_from_isr();
    }

    return task;
}

int trt_wait_q_wake_all_locked(trt_wait_q_t *queue)
{
    int count = 0;

    if (queue == 0)
    {
        return 0;
    }

    while (trt_wait_q_wake_one_locked(queue) != 0)
    {
        count++;
    }

    return count;
}

int trt_wait_q_wake_all(trt_wait_q_t *queue)
{
    int count;
    critical_state_t state;

    state = critical_enter();
    count = trt_wait_q_wake_all_locked(queue);
    critical_exit(state);

    return count;
}

int trt_wait_q_wake_all_from_isr(trt_wait_q_t *queue)
{
    int count;

    if (!arch_in_isr())
    {
        return 0;
    }

    count = trt_wait_q_wake_all_locked(queue);
    if (count != 0)
    {
        sched_request_from_isr();
    }

    return count;
}
