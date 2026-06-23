#include "mutex.h"
#include "critical.h"
#include "error.h"
#include "hal.h"
#include "malloc.h"
#include "sched.h"
#include "task.h"
#include "wait_queue.h"

typedef struct
{
    task_t *owner;
    uint32_t lock_count;
    uint8_t destroyed;
    trt_wait_q_t waiters;
} trt_mutex_t;

static err_t mutex_lookup(trt_handle_t handle, uint32_t rights, trt_mutex_t **out)
{
    void *object;
    err_t result;

    result = trt_handle_lookup(handle, TRT_OBJ_MUTEX, rights, &object);
    if (result != ERR_OK)
    {
        return result;
    }

    *out = object;
    return ERR_OK;
}

static void mutex_init_obj(trt_mutex_t *mutex)
{
    mutex->owner = 0;
    mutex->lock_count = 0;
    mutex->destroyed = 0;
    INIT_LIST_HEAD(&mutex->waiters.waiters);
}

trt_handle_t trt_mutex_create(void)
{
    trt_mutex_t *mutex;
    trt_handle_t handle;

    mutex = malloc(sizeof(*mutex));
    if (mutex == 0)
    {
        return TRT_HANDLE_INVALID;
    }

    mutex_init_obj(mutex);
    if (trt_handle_alloc(mutex, TRT_OBJ_MUTEX, TRT_RIGHT_WAIT | TRT_RIGHT_POST | TRT_RIGHT_DESTROY,
                         &handle) != ERR_OK)
    {
        free(mutex);
        return TRT_HANDLE_INVALID;
    }

    return handle;
}

static err_t mutex_destroy_obj(trt_mutex_t *mutex, int *woken)
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
    *woken = trt_wait_q_wake_all_result_locked(&mutex->waiters, TASK_WAIT_DESTROYED);
    critical_exit(state);

    return ERR_OK;
}

err_t trt_mutex_destroy(trt_handle_t handle)
{
    trt_mutex_t *mutex;
    err_t result;
    int woken = 0;

    if (arch_in_isr())
    {
        return ERR_STATE;
    }

    result = mutex_lookup(handle, TRT_RIGHT_DESTROY, &mutex);
    if (result != ERR_OK)
    {
        return result;
    }

    result = mutex_destroy_obj(mutex, &woken);
    if (result != ERR_OK)
    {
        return result;
    }

    trt_handle_close(handle);
    free(mutex);
    if (woken != 0)
    {
        task_yield();
    }
    return ERR_OK;
}

static err_t mutex_trylock_obj(trt_mutex_t *mutex)
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

err_t trt_mutex_trylock(trt_handle_t handle)
{
    trt_mutex_t *mutex;
    err_t result;

    result = mutex_lookup(handle, TRT_RIGHT_WAIT, &mutex);
    if (result != ERR_OK)
    {
        return result;
    }

    return mutex_trylock_obj(mutex);
}

static err_t mutex_lock_obj(trt_mutex_t *mutex)
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

err_t trt_mutex_lock(trt_handle_t handle)
{
    trt_mutex_t *mutex;
    err_t result;

    result = mutex_lookup(handle, TRT_RIGHT_WAIT, &mutex);
    if (result != ERR_OK)
    {
        return result;
    }

    return mutex_lock_obj(mutex);
}

static err_t mutex_lock_timeout_obj(trt_mutex_t *mutex, trt_time_t timeout)
{
    task_t *current;
    int result;
    critical_state_t state;

    if (timeout.us == TRT_TIME_FOREVER_US)
    {
        return mutex_lock_obj(mutex);
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

err_t trt_mutex_lock_timeout(trt_handle_t handle, trt_time_t timeout)
{
    trt_mutex_t *mutex;
    err_t result;

    result = mutex_lookup(handle, TRT_RIGHT_WAIT, &mutex);
    if (result != ERR_OK)
    {
        return result;
    }

    return mutex_lock_timeout_obj(mutex, timeout);
}

static err_t mutex_unlock_obj(trt_mutex_t *mutex)
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

err_t trt_mutex_unlock(trt_handle_t handle)
{
    trt_mutex_t *mutex;
    err_t result;

    result = mutex_lookup(handle, TRT_RIGHT_POST, &mutex);
    if (result != ERR_OK)
    {
        return result;
    }

    return mutex_unlock_obj(mutex);
}
