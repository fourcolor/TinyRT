#include "mutex.h"
#include "critical.h"
#include "error.h"
#include "sched.h"
#include "task.h"
#include "wait_queue.h"

void trt_mutex_init(trt_mutex_t *mutex)
{
    mutex->owner = 0;
    mutex->lock_count = 0;
    mutex->destroyed = 0;
    INIT_LIST_HEAD(&mutex->waiters.waiters);
}

err_t trt_mutex_destroy(trt_mutex_t *mutex)
{
    critical_state_t state;

    if (mutex == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    if (mutex->destroyed)
    {
        critical_exit(state);
        return ERR_STATE;
    }

    mutex->destroyed = 1;
    mutex->owner = 0;
    mutex->lock_count = 0;
    trt_wait_q_wake_all_result_locked(&mutex->waiters, TASK_WAIT_DESTROYED);
    critical_exit(state);

    return ERR_OK;
}

err_t trt_mutex_trylock(trt_mutex_t *mutex)
{
    task_t *current;
    critical_state_t state;

    if (mutex == 0 || scheduler.current_task == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    current = scheduler.current_task;

    if (mutex->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (mutex->owner == 0)
    {
        mutex->owner = current;
        mutex->lock_count = 1;
        critical_exit(state);
        return ERR_OK;
    }

    if (mutex->owner == current)
    {
        mutex->lock_count++;
        critical_exit(state);
        return ERR_OK;
    }

    critical_exit(state);
    return ERR_BUSY;
}

err_t trt_mutex_lock(trt_mutex_t *mutex)
{
    task_t *current;
    int result;
    critical_state_t state;

    if (mutex == 0 || scheduler.current_task == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    current = scheduler.current_task;

    if (mutex->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (mutex->owner == 0)
    {
        mutex->owner = current;
        mutex->lock_count = 1;
        critical_exit(state);
        return ERR_OK;
    }

    if (mutex->owner == current)
    {
        mutex->lock_count++;
        critical_exit(state);
        return ERR_OK;
    }

    result = trt_wait_q_block_locked(&mutex->waiters);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    if (scheduler.current_task->wait_result == TASK_WAIT_DESTROYED)
    {
        return ERR_DESTROYED;
    }
    return mutex->owner == scheduler.current_task ? ERR_OK : ERR_STATE;
}

err_t trt_mutex_lock_timeout(trt_mutex_t *mutex, trt_time_t timeout)
{
    task_t *current;
    int result;
    critical_state_t state;

    if (timeout.us == TRT_TIME_FOREVER_US)
    {
        return trt_mutex_lock(mutex);
    }

    if (mutex == 0 || scheduler.current_task == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    current = scheduler.current_task;

    if (mutex->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (mutex->owner == 0)
    {
        mutex->owner = current;
        mutex->lock_count = 1;
        critical_exit(state);
        return ERR_OK;
    }

    if (mutex->owner == current)
    {
        mutex->lock_count++;
        critical_exit(state);
        return ERR_OK;
    }

    result = trt_wait_q_block_timeout_locked(&mutex->waiters, timeout);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    if (scheduler.current_task->wait_result == TASK_WAIT_DESTROYED)
    {
        return ERR_DESTROYED;
    }
    return (scheduler.current_task->wait_result == TASK_WAIT_OBJECT &&
            mutex->owner == scheduler.current_task)
               ? ERR_OK
               : ERR_TIMEOUT;
}

err_t trt_mutex_unlock(trt_mutex_t *mutex)
{
    task_t *next;
    critical_state_t state;

    if (mutex == 0 || scheduler.current_task == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();

    if (mutex->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (mutex->owner != scheduler.current_task || mutex->lock_count == 0)
    {
        critical_exit(state);
        return ERR_PERM;
    }

    mutex->lock_count--;
    if (mutex->lock_count != 0)
    {
        critical_exit(state);
        return ERR_OK;
    }

    next = trt_wait_q_wake_one_locked(&mutex->waiters);
    if (next != 0)
    {
        mutex->owner = next;
        mutex->lock_count = 1;
    }
    else
    {
        mutex->owner = 0;
    }

    critical_exit(state);
    return ERR_OK;
}
