#include "sched.h"
#include "arch_port.h"
#include "critical.h"
#include "list.h"
#include "logger.h"
#include "timer.h"

scheduler_t scheduler;

void sched_init(void)
{
    int priority;

    if (scheduler.initialized)
    {
        return;
    }

    INIT_LIST_HEAD(&scheduler.master_task_list);
    INIT_LIST_HEAD(&scheduler.deleted_task_list);
    INIT_LIST_HEAD(&scheduler.timeout_task_list);
    for (priority = RTOS_PRIORITY_MIN; priority <= RTOS_PRIORITY_MAX; priority++)
    {
        INIT_LIST_HEAD(&scheduler.ready_tasks.list[priority]);
    }

    scheduler.ready_tasks.mask = 0;
    scheduler.idle_task = 0;
    scheduler.time_slice_ticks = 0;
    scheduler.current_task = 0;
    scheduler.started = 0;
    scheduler.lock_count = 0;
    scheduler.resched_pending = 0;
    scheduler.initialized = 1;
}

void sched_add_task(task_t *task)
{
    list_add_tail(&task->master_list, &scheduler.master_task_list);
    sched_ready_enqueue(task);
}

void sched_set_idle_task(task_t *task)
{
    scheduler.idle_task = task;
}

void sched_ready_enqueue(task_t *task)
{
    if (task == 0 || task->priority > RTOS_PRIORITY_MAX || task->state == TASK_DELETED)
    {
        return;
    }

    if (!list_empty(&task->state_list))
    {
        return;
    }

    list_add_tail(&task->state_list, &scheduler.ready_tasks.list[task->priority]);
    scheduler.ready_tasks.mask |= (1u << task->priority);
}

void sched_ready_remove(task_t *task)
{
    if (task == 0 || task->priority > RTOS_PRIORITY_MAX)
    {
        return;
    }

    if (list_empty(&task->state_list))
    {
        return;
    }

    list_del_init(&task->state_list);
    if (list_empty(&scheduler.ready_tasks.list[task->priority]))
    {
        scheduler.ready_tasks.mask &= ~(1u << task->priority);
    }
}

void sched_mark_deleted(task_t *task)
{
    if (task == 0 || task == scheduler.idle_task || task->state == TASK_DELETED)
    {
        return;
    }

    if (!list_empty(&task->state_list))
    {
        if (task->state == TASK_READY)
        {
            sched_ready_remove(task);
        }
        else
        {
            list_del_init(&task->state_list);
        }
    }
    sched_timeout_remove(task);

    task->waiting_on_timer = 0;
    task->wait_result = TASK_WAIT_NONE;
    task->state = TASK_DELETED;
    list_add_tail(&task->state_list, &scheduler.deleted_task_list);
}

task_t *sched_pop_deleted(void)
{
    task_t *task;

    if (list_empty(&scheduler.deleted_task_list))
    {
        return 0;
    }

    task = list_first_entry(&scheduler.deleted_task_list, task_t, state_list);
    list_del_init(&task->state_list);
    list_del_init(&task->master_list);

    return task;
}

void sched_block_current(list_head_t *waiters)
{
    list_head_t *pos;
    task_t *task;

    if (scheduler.current_task == 0 || waiters == 0)
    {
        return;
    }

    task = scheduler.current_task;
    task->state = TASK_BLOCKED;
    task->waiting_on_timer = 0;
    if (!list_empty(&task->state_list))
    {
        sched_ready_remove(task);
    }

    list_for_each(pos, waiters)
    {
        task_t *waiter = list_entry(pos, task_t, state_list);

        if (waiter->priority < task->priority)
        {
            list_add_before(&task->state_list, pos);
            return;
        }
    }

    list_add_tail(&task->state_list, waiters);
}

void sched_timeout_insert(task_t *task)
{
    list_head_t *pos;

    if (task == 0 || !task->waiting_on_timer)
    {
        return;
    }

    if (!list_empty(&task->timeout_list))
    {
        list_del_init(&task->timeout_list);
    }

    list_for_each(pos, &scheduler.timeout_task_list)
    {
        task_t *queued = list_entry(pos, task_t, timeout_list);

        if ((int32_t)(task->wakeup - queued->wakeup) < 0)
        {
            list_add_before(&task->timeout_list, pos);
            return;
        }
    }

    list_add_tail(&task->timeout_list, &scheduler.timeout_task_list);
}

void sched_timeout_remove(task_t *task)
{
    if (task == 0 || list_empty(&task->timeout_list))
    {
        return;
    }

    list_del_init(&task->timeout_list);
}

task_t *sched_wake_one(list_head_t *waiters)
{
    task_t *task;

    if (waiters == 0 || list_empty(waiters))
    {
        return 0;
    }

    task = list_first_entry(waiters, task_t, state_list);
    list_del_init(&task->state_list);
    sched_timeout_remove(task);
    task->waiting_on_timer = 0;
    task->state = TASK_READY;
    sched_ready_enqueue(task);

    return task;
}

static task_t *sched_ready_pop_highest(void)
{
    list_head_t *head;
    list_head_t *node;
    task_t *task;
    uint32_t priority;

    if (scheduler.ready_tasks.mask == 0)
    {
        return 0;
    }

    priority = 31u - (uint32_t)__builtin_clz(scheduler.ready_tasks.mask);
    head = &scheduler.ready_tasks.list[priority];

    if (list_empty(head))
    {
        LOG_ERROR("ready queue corrupt: priority=%lu mask=0x%08lx\n", priority,
                  scheduler.ready_tasks.mask);
        for (;;)
            ;
    }

    node = head->next;
    task = list_entry(node, task_t, state_list);
    list_del_init(node);
    if (list_empty(head))
    {
        scheduler.ready_tasks.mask &= ~(1u << priority);
    }

    return task;
}

task_t *sched_pick_next(void)
{
    task_t *previous = scheduler.current_task;
    task_t *task;

    if (scheduler.current_task != 0 && scheduler.current_task->state == TASK_RUNNING)
    {
        scheduler.current_task->state = TASK_READY;
        if (scheduler.current_task != scheduler.idle_task)
        {
            sched_ready_enqueue(scheduler.current_task);
        }
    }

    task = sched_ready_pop_highest();
    if (task != 0)
    {
        task->state = TASK_RUNNING;
        LOG_DEBUG("switch current=%p next=%p next_entry=%p next_sp=%p\n", previous, task,
                  arch_task_frame_entry(task->sp), task->sp);
        return task;
    }

    if (scheduler.idle_task != 0)
    {
        scheduler.idle_task->state = TASK_RUNNING;
        return scheduler.idle_task;
    }

    return previous;
}

int sched_on_tick(void)
{
#if RTOS_SCHED_PREEMPTIVE
    uint32_t higher_priority_mask;

    if (scheduler.current_task == 0)
    {
        return 0;
    }

    if (scheduler.current_task->state != TASK_RUNNING || sched_current_is_idle())
    {
        scheduler.time_slice_ticks = 0;
        return 1;
    }

    if (scheduler.ready_tasks.mask == 0)
    {
        return 0;
    }

    if (scheduler.current_task->priority < RTOS_PRIORITY_MAX)
    {
        higher_priority_mask = ~((1u << (scheduler.current_task->priority + 1u)) - 1u);
        if ((scheduler.ready_tasks.mask & higher_priority_mask) != 0)
        {
            scheduler.time_slice_ticks = 0;
            return 1;
        }
    }

#if RTOS_SCHED_ROUND_ROBIN
    if (list_empty(&scheduler.ready_tasks.list[scheduler.current_task->priority]))
    {
        return 0;
    }

    scheduler.time_slice_ticks++;
    if (scheduler.time_slice_ticks >= RTOS_TIME_SLICE_TICKS)
    {
        scheduler.time_slice_ticks = 0;
        return 1;
    }
#else
#endif
#endif

    return 0;
}

void sched_wake_expired(void)
{
    uint32_t now = timer_ticks();

    while (!list_empty(&scheduler.timeout_task_list))
    {
        task_t *task = list_first_entry(&scheduler.timeout_task_list, task_t, timeout_list);

        if (!timer_expired(now, task->wakeup))
        {
            return;
        }

        list_del_init(&task->timeout_list);
        if (task->state != TASK_BLOCKED || !task->waiting_on_timer)
        {
            continue;
        }

        task->waiting_on_timer = 0;
        task->wait_result = TASK_WAIT_TIMEOUT;
        if (!list_empty(&task->state_list))
        {
            list_del_init(&task->state_list);
        }
        task->state = TASK_READY;
        sched_ready_enqueue(task);
    }
}

int sched_current_is_idle(void)
{
    return scheduler.current_task != 0 && scheduler.current_task == scheduler.idle_task;
}

void sched_dump_all_task(void)
{
    list_head_t *pos;
    list_for_each(pos, &scheduler.master_task_list)
    {
        task_t *task = list_entry(pos, task_t, master_list);
        LOG_INFO(
            "Task: %s, sp: %ld, stack base: %ld, stack size: %ld, task state: %d, priority: %d",
            task->name, task->sp, task->stack_base, task->stack_size, task->state, task->priority);
    }
}

void sched_lock(void)
{
    critical_state_t state = critical_enter();

    scheduler.lock_count++;

    critical_exit(state);
}

void sched_unlock(void)
{
    int need_reschedule = 0;
    critical_state_t state = critical_enter();

    if (scheduler.lock_count == 0)
    {
        critical_exit(state);
        return;
    }

    if (scheduler.lock_count != 0)
    {
        scheduler.lock_count--;
    }

    if (scheduler.lock_count == 0 && scheduler.resched_pending)
    {
        scheduler.resched_pending = 0;
        need_reschedule = 1;
    }

    critical_exit(state);

    if (need_reschedule && scheduler.started && scheduler.current_task != 0)
    {
        task_yield();
    }
}

int sched_is_locked(void)
{
    return scheduler.lock_count != 0;
}

static int sched_has_higher_priority_ready(void)
{
    uint32_t higher_priority_mask;

    if (scheduler.current_task == 0 || scheduler.current_task->priority >= RTOS_PRIORITY_MAX)
    {
        return 0;
    }

    higher_priority_mask = ~((1u << (scheduler.current_task->priority + 1u)) - 1u);
    return (scheduler.ready_tasks.mask & higher_priority_mask) != 0;
}

void sched_set_pending(void)
{
    critical_state_t state = critical_enter();

    scheduler.resched_pending = 1;

    critical_exit(state);
}

void sched_request_from_isr(void)
{
    sched_set_pending();
}

int sched_try_reschedule_from_trap(void)
{
    int need_reschedule = 0;
    critical_state_t state = critical_enter();

    if (scheduler.started && scheduler.current_task != 0 && scheduler.resched_pending &&
        scheduler.lock_count == 0)
    {
        scheduler.resched_pending = 0;
        if (scheduler.current_task->state != TASK_RUNNING || sched_current_is_idle() ||
            sched_has_higher_priority_ready())
        {
            need_reschedule = 1;
        }
    }

    critical_exit(state);

    if (need_reschedule)
    {
        scheduler.current_task = sched_pick_next();
    }

    return need_reschedule;
}
