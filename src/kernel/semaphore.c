#include "semaphore.h"
#include "critical.h"
#include "malloc.h"
#include "error.h"
#include "hal.h"
#include "sched.h"
#include "task.h"
#include "wait_queue.h"

typedef struct
{
    int cnt;
    int max;
    uint8_t destroyed;
    trt_wait_q_t waiters;
} trt_sem_t;

static err_t sem_lookup(trt_handle_t handle, uint32_t rights, trt_sem_t **out)
{
    void *object;
    err_t result;

    result = trt_handle_lookup(handle, TRT_OBJ_SEM, rights, &object);
    if (result != ERR_OK)
    {
        return result;
    }

    *out = object;
    return ERR_OK;
}

static void sem_init_obj(trt_sem_t *sem, int max, int cnt)
{
    sem->cnt = (cnt < 0) ? 0 : cnt;
    sem->max = (max < cnt) ? cnt : max;
    sem->destroyed = 0;
    INIT_LIST_HEAD(&(sem->waiters.waiters));
}

trt_handle_t trt_sem_create(int max, int cnt)
{
    trt_sem_t *sem = malloc(sizeof(trt_sem_t));
    trt_handle_t handle;

    if (!sem)
    {
        return TRT_HANDLE_INVALID;
    }

    sem_init_obj(sem, max, cnt);
    if (trt_handle_alloc(sem, TRT_OBJ_SEM, TRT_RIGHT_WAIT | TRT_RIGHT_POST | TRT_RIGHT_DESTROY,
                         &handle) != ERR_OK)
    {
        free(sem);
        return TRT_HANDLE_INVALID;
    }

    return handle;
}

static err_t sem_destroy_obj(trt_sem_t *sem)
{
    critical_state_t state;

    if (sem == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    if (sem->destroyed)
    {
        critical_exit(state);
        return ERR_STATE;
    }

    sem->destroyed = 1;
    sem->cnt = 0;
    trt_wait_q_wake_all_result_locked(&sem->waiters, TASK_WAIT_DESTROYED);
    critical_exit(state);

    return ERR_OK;
}

err_t trt_sem_destroy(trt_handle_t handle)
{
    trt_sem_t *sem;
    err_t result;

    result = sem_lookup(handle, TRT_RIGHT_DESTROY, &sem);
    if (result != ERR_OK)
    {
        return result;
    }

    result = sem_destroy_obj(sem);
    if (result != ERR_OK)
    {
        return result;
    }

    trt_handle_close(handle);
    free(sem);
    return ERR_OK;
}

static err_t sem_post_obj(trt_sem_t *sem)
{
    critical_state_t state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
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

err_t trt_sem_post(trt_handle_t handle)
{
    trt_sem_t *sem;
    err_t result;

    result = sem_lookup(handle, TRT_RIGHT_POST, &sem);
    if (result != ERR_OK)
    {
        return result;
    }

    return sem_post_obj(sem);
}

static err_t sem_post_from_isr_obj(trt_sem_t *sem)
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

    if (sem->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
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

err_t trt_sem_post_from_isr(trt_handle_t handle)
{
    trt_sem_t *sem;
    err_t result;

    result = sem_lookup(handle, TRT_RIGHT_POST, &sem);
    if (result != ERR_OK)
    {
        return result;
    }

    return sem_post_from_isr_obj(sem);
}

static err_t sem_wait_obj(trt_sem_t *sem)
{
    int result;
    critical_state_t state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
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

err_t trt_sem_wait(trt_handle_t handle)
{
    trt_sem_t *sem;
    err_t result;

    result = sem_lookup(handle, TRT_RIGHT_WAIT, &sem);
    if (result != ERR_OK)
    {
        return result;
    }

    return sem_wait_obj(sem);
}

static err_t sem_wait_timeout_obj(trt_sem_t *sem, trt_time_t timeout)
{
    int result;
    critical_state_t state = critical_enter();

    if (timeout.us == TRT_TIME_FOREVER_US)
    {
        critical_exit(state);
        return sem_wait_obj(sem);
    }

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
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

err_t trt_sem_wait_timeout(trt_handle_t handle, trt_time_t timeout)
{
    trt_sem_t *sem;
    err_t result;

    result = sem_lookup(handle, TRT_RIGHT_WAIT, &sem);
    if (result != ERR_OK)
    {
        return result;
    }

    return sem_wait_timeout_obj(sem, timeout);
}

static err_t sem_trywait_obj(trt_sem_t *sem)
{
    critical_state_t state = critical_enter();

    if (!sem)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (sem->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
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

err_t trt_sem_trywait(trt_handle_t handle)
{
    trt_sem_t *sem;
    err_t result;

    result = sem_lookup(handle, TRT_RIGHT_WAIT, &sem);
    if (result != ERR_OK)
    {
        return result;
    }

    return sem_trywait_obj(sem);
}
