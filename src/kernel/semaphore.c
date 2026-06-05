#include "semaphore.h"
#include "critical.h"
#include "malloc.h"
#include "error.h"
#include "hal.h"
#include "sched.h"
#include "task.h"
#include "wait_queue.h"

void trt_sem_init(trt_sem_t *sem, int max, int cnt)
{
    sem->cnt = (cnt < 0) ? 0 : cnt;
    sem->max = (max < cnt) ? cnt : max;
    INIT_LIST_HEAD(&(sem->waiters.waiters));
}

trt_sem_t *trt_sem_create(int max, int cnt)
{
    trt_sem_t *sem = malloc(sizeof(trt_sem_t));
    if (!sem)
    {
        return NULL;
    }

    trt_sem_init(sem, max, cnt);
    return sem;
}

err_t trt_sem_post(trt_sem_t *sem)
{
    critical_state_t state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (!trt_wait_q_empty(&sem->waiters))
    {
        trt_wait_q_wake_one_locked(&sem->waiters);
        critical_exit(state);
        return ERR_OK;
    }

    if (sem->cnt == sem->max)
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    sem->cnt++;
    critical_exit(state);
    return ERR_OK;
}

err_t trt_sem_post_from_isr(trt_sem_t *sem)
{
    critical_state_t state;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (!trt_wait_q_empty(&sem->waiters))
    {
        trt_wait_q_wake_one_from_isr(&sem->waiters);
        critical_exit(state);
        return ERR_OK;
    }

    if (sem->cnt == sem->max)
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    sem->cnt++;
    sched_request_from_isr();
    critical_exit(state);
    return ERR_OK;
}

err_t trt_sem_wait(trt_sem_t *sem)
{
    int result;
    critical_state_t state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->cnt > 0)
    {
        sem->cnt--;
        critical_exit(state);
        return ERR_OK;
    }

    if (sched_is_locked())
    {
        sched_set_pending();
        critical_exit(state);
        return ERR_LOCKED;
    }

    result = trt_wait_q_block_locked(&sem->waiters);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();

    return scheduler.current_task->wait_result == TASK_WAIT_OBJECT ? ERR_OK : ERR_STATE;
}

err_t trt_sem_wait_timeout(trt_sem_t *sem, trt_time_t timeout)
{
    int result;
    critical_state_t state = critical_enter();

    if (timeout.us == TRT_TIME_FOREVER_US)
    {
        critical_exit(state);
        return trt_sem_wait(sem);
    }

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->cnt > 0)
    {
        sem->cnt--;
        critical_exit(state);
        return ERR_OK;
    }

    if (sched_is_locked())
    {
        sched_set_pending();
        critical_exit(state);
        return ERR_LOCKED;
    }

    result = trt_wait_q_block_timeout_locked(&sem->waiters, timeout);
    critical_exit(state);

    if (result != 0)
    {
        return result;
    }

    task_yield();
    return scheduler.current_task->wait_result == TASK_WAIT_OBJECT ? ERR_OK : ERR_TIMEOUT;
}

err_t trt_sem_trywait(trt_sem_t *sem)
{
    critical_state_t state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->cnt == 0)
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    sem->cnt--;
    critical_exit(state);
    return ERR_OK;
}
