#include "task.h"
#include <string.h>
#include "hal.h"
#include "port.h"
#include "logger.h"
#include "malloc.h"
#include "rtos_config.h"
#include "sched.h"
#include "timer.h"

static uint32_t task_count;
static int task_initialized;

void task_init(void)
{
    if (task_initialized)
    {
        return;
    }

    sched_init();
    task_count = 0;
    task_initialized = 1;
}

static void task_set_name(task_t *task, const char *name)
{
    if (name == 0)
    {
        name = "";
    }

    strncpy(task->name, name, sizeof(task->name) - 1u);
    task->name[sizeof(task->name) - 1u] = '\0';
}

static void task_exit_trap(void)
{
    task_exit();
}

static err_t task_lookup(trt_handle_t handle, uint32_t rights, task_t **out)
{
    void *object;
    err_t result;

    result = trt_handle_lookup(handle, TRT_OBJ_TASK, rights, &object);
    if (result != ERR_OK)
    {
        return result;
    }

    *out = object;
    return ERR_OK;
}

task_t *task_create_kernel(const char *name, void (*entry)(void *), void *arg, size_t stack_size,
                           uint8_t priority)
{
    uint8_t *stack;
    task_t *task;

    if (!scheduler.started)
    {
        arch_interrupt_disable();
    }

    if (entry == 0 || stack_size < ARCH_TASK_CONTEXT_SIZE || priority > RTOS_PRIORITY_MAX)
    {
        return 0;
    }

    if (task_count >= RTOS_TASK_MAX)
    {
        return 0;
    }

    stack_size = (stack_size + 15u) & ~(size_t)15u;
    task = malloc(sizeof(*task));
    if (task == 0)
    {
        LOG_WARN("task_create failed name=%s reason=no_tcb heap_free=%lu\n", name,
                 heap_free_size());
        return 0;
    }

    stack = malloc(stack_size);
    if (stack == 0)
    {
        LOG_WARN("task_create failed name=%s reason=no_stack heap_free=%lu\n", name,
                 heap_free_size());
        free(task);
        return 0;
    }

    task_set_name(task, name);
    task->stack_base = (uint32_t *)stack;
    task->stack_size = stack_size;
    task->wakeup = 0;
    task->waiting_on_timer = 0;
    task->wait_result = TASK_WAIT_NONE;
    task->priority = priority;
    task->base_priority = priority;
    task->effective_priority = priority;
    task->handle = TRT_HANDLE_INVALID;
    task->entry = entry;
    task->arg = arg;
    task->sp = arch_task_init_frame(stack, stack_size, entry, arg, task_exit_trap);
    task->state = TASK_READY;
    INIT_LIST_HEAD(&task->master_list);
    INIT_LIST_HEAD(&task->state_list);
    INIT_LIST_HEAD(&task->timeout_list);
    sched_add_task(task);
    task_count++;

    return task;
}

trt_handle_t task_create(const char *name, void (*entry)(void *), void *arg, size_t stack_size,
                         uint8_t priority)
{
    task_t *task;
    trt_handle_t handle;

    task = task_create_kernel(name, entry, arg, stack_size, priority);
    if (task == 0)
    {
        return TRT_HANDLE_INVALID;
    }

    if (trt_handle_alloc(task, TRT_OBJ_TASK, TRT_RIGHT_DESTROY, &handle) != ERR_OK)
    {
        task_delete_kernel(task);
        return TRT_HANDLE_INVALID;
    }

    task->handle = handle;
    return handle;
}

static err_t task_delete_obj(task_t *task)
{
    int delete_self;
    critical_state_t state;

    state = critical_enter();

    if (task == 0)
    {
        task = scheduler.current_task;
    }

    if (task == 0 || task == scheduler.idle_task)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    delete_self = (task == scheduler.current_task);
    if (task->state == TASK_DELETED)
    {
        critical_exit(state);
        return ERR_STATE;
    }
    if (delete_self && sched_is_locked())
    {
        critical_exit(state);
        return ERR_LOCKED;
    }

    if (task->handle != TRT_HANDLE_INVALID)
    {
        trt_handle_close(task->handle);
        task->handle = TRT_HANDLE_INVALID;
    }

    sched_mark_deleted(task);
    if (task_count != 0)
    {
        task_count--;
    }
    if (delete_self)
    {
        scheduler.resched_pending = 1;
    }

    critical_exit(state);

    if (delete_self)
    {
        arch_yield();
        for (;;)
        {
            arch_yield();
        }
    }

    return ERR_OK;
}

err_t task_delete_kernel(task_t *task)
{
    return task_delete_obj(task);
}

err_t task_delete(trt_handle_t handle)
{
    task_t *task;
    err_t result;

    result = task_lookup(handle, TRT_RIGHT_DESTROY, &task);
    if (result != ERR_OK)
    {
        return result;
    }

    return task_delete_obj(task);
}

void task_exit(void)
{
    task_delete_obj(0);
    for (;;)
    {
        arch_yield();
    }
}

void task_cleanup_deleted(void)
{
    for (;;)
    {
        task_t *task;
        critical_state_t state;

        state = critical_enter();
        task = sched_pop_deleted();
        critical_exit(state);

        if (task == 0)
        {
            return;
        }

        free(task->stack_base);
        free(task);
    }
}

void __attribute__((noinline)) task_yield(void)
{
    if (sched_is_locked())
    {
        sched_set_pending();
        return;
    }

    if (scheduler.current_task != 0 && !sched_current_is_idle() &&
        scheduler.current_task->state == TASK_RUNNING)
    {
        scheduler.current_task->state = TASK_READY;
        sched_ready_enqueue(scheduler.current_task);
    }

    arch_yield();
}

void task_delay_ticks(uint32_t ticks)
{
    uint32_t wakeup;
    critical_state_t state;

    if (scheduler.current_task == 0)
    {
        return;
    }

    if (sched_is_locked())
    {
        sched_set_pending();
        return;
    }

    wakeup = timer_ticks() + ticks;

    while (!timer_expired(timer_ticks(), wakeup))
    {
        state = critical_enter();
        if (timer_expired(timer_ticks(), wakeup))
        {
            critical_exit(state);
            break;
        }

        scheduler.current_task->wakeup = wakeup;
        scheduler.current_task->waiting_on_timer = 1;
        scheduler.current_task->wait_result = TASK_WAIT_NONE;
        scheduler.current_task->state = TASK_BLOCKED;
        sched_ready_remove(scheduler.current_task);
        sched_timeout_insert(scheduler.current_task);
        critical_exit(state);
        task_yield();
    }
}

void task_sleep(trt_time_t delay)
{
    task_delay_ticks(timer_us_to_ticks(delay.us));
}
