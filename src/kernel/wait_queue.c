#include "wait_queue.h"
#include "critical.h"
#include "error.h"
#include "hal.h"
#include "sched.h"
#include "task.h"
#include "timer.h"

void trt_wait_q_init(trt_wait_q_t *wq)
{
    INIT_LIST_HEAD(&wq->waiters);
}

int trt_wait_q_empty(trt_wait_q_t *wq)
{
    return list_empty(&wq->waiters);
}

err_t trt_wait_q_block_locked(trt_wait_q_t *wq)
{
    if (wq == 0 || scheduler.current_task == 0 || sched_is_locked())
    {
        sched_set_pending();
        return wq == 0 || scheduler.current_task == 0 ? ERR_INVAL : ERR_LOCKED;
    }

    scheduler.current_task->wait_result = TASK_WAIT_NONE;
    sched_block_current(&wq->waiters);
    return ERR_OK;
}

err_t trt_wait_q_block(trt_wait_q_t *wq)
{
    int result;
    critical_state_t state;

    state = critical_enter();
    result = trt_wait_q_block_locked(wq);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    if (scheduler.current_task->wait_result == TASK_WAIT_OBJECT)
    {
        return ERR_OK;
    }
    if (scheduler.current_task->wait_result == TASK_WAIT_DESTROYED)
    {
        return ERR_DESTROYED;
    }
    return ERR_STATE;
}

static err_t trt_wait_q_block_ticks_locked(trt_wait_q_t *wq, uint32_t timeout_ticks)
{
    int result;

    if (timeout_ticks == 0)
    {
        return ERR_TIMEOUT;
    }

    result = trt_wait_q_block_locked(wq);
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

static err_t trt_wait_q_block_ticks(trt_wait_q_t *wq, uint32_t timeout_ticks)
{
    int result;
    critical_state_t state;

    state = critical_enter();
    result = trt_wait_q_block_ticks_locked(wq, timeout_ticks);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    if (scheduler.current_task->wait_result == TASK_WAIT_OBJECT)
    {
        return ERR_OK;
    }
    if (scheduler.current_task->wait_result == TASK_WAIT_DESTROYED)
    {
        return ERR_DESTROYED;
    }
    return ERR_TIMEOUT;
}

err_t trt_wait_q_block_timeout_locked(trt_wait_q_t *wq, trt_time_t timeout)
{
    if (timeout.us == TRT_TIME_FOREVER_US)
    {
        return trt_wait_q_block_locked(wq);
    }

    return trt_wait_q_block_ticks_locked(wq, timer_us_to_ticks(timeout.us));
}

err_t trt_wait_q_block_timeout(trt_wait_q_t *wq, trt_time_t timeout)
{
    if (timeout.us == TRT_TIME_FOREVER_US)
    {
        return trt_wait_q_block(wq);
    }

    return trt_wait_q_block_ticks(wq, timer_us_to_ticks(timeout.us));
}

task_t *trt_wait_q_wake_one_result_locked(trt_wait_q_t *wq, task_wait_result_t result)
{
    task_t *task;

    if (wq == 0)
    {
        return 0;
    }

    task = sched_wake_one(&wq->waiters);
    if (task != 0 && sched_is_locked())
    {
        sched_set_pending();
    }
    if (task != 0)
    {
        task->wait_result = result;
    }

    return task;
}

task_t *trt_wait_q_wake_one_locked(trt_wait_q_t *wq)
{
    return trt_wait_q_wake_one_result_locked(wq, TASK_WAIT_OBJECT);
}

task_t *trt_wait_q_wake_one(trt_wait_q_t *wq)
{
    task_t *task;
    critical_state_t state;

    state = critical_enter();
    task = trt_wait_q_wake_one_locked(wq);
    critical_exit(state);

    return task;
}

task_t *trt_wait_q_wake_one_from_isr(trt_wait_q_t *wq)
{
    task_t *task;

    if (!arch_in_isr())
    {
        return 0;
    }

    task = trt_wait_q_wake_one_locked(wq);
    if (task != 0)
    {
        sched_request_from_isr();
    }

    return task;
}

int trt_wait_q_wake_all_result_locked(trt_wait_q_t *wq, task_wait_result_t result)
{
    int count = 0;

    if (wq == 0)
    {
        return 0;
    }

    while (trt_wait_q_wake_one_result_locked(wq, result) != 0)
    {
        count++;
    }

    return count;
}

int trt_wait_q_wake_all_locked(trt_wait_q_t *wq)
{
    return trt_wait_q_wake_all_result_locked(wq, TASK_WAIT_OBJECT);
}

int trt_wait_q_wake_all(trt_wait_q_t *wq)
{
    int count;
    critical_state_t state;

    state = critical_enter();
    count = trt_wait_q_wake_all_locked(wq);
    critical_exit(state);

    return count;
}

int trt_wait_q_wake_all_from_isr(trt_wait_q_t *wq)
{
    int count;

    if (!arch_in_isr())
    {
        return 0;
    }

    count = trt_wait_q_wake_all_locked(wq);
    if (count != 0)
    {
        sched_request_from_isr();
    }

    return count;
}
