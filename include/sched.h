#pragma once

#include "task.h"
#include "rtos_config.h"

typedef struct
{
    list_head_t list[RTOS_PRIORITY_MAX + 1];
    uint32_t mask;
} ready_task_container_t;

typedef struct
{
    task_t *current_task;
    task_t *idle_task;
    list_head_t master_task_list;
    list_head_t deleted_task_list;
    list_head_t timeout_task_list;
    ready_task_container_t ready_tasks;
    uint32_t time_slice_ticks;
    int initialized;
    volatile int started;
    volatile uint32_t lock_count;
    uint8_t resched_pending;
} scheduler_t;

extern scheduler_t scheduler;

void sched_init(void);
void sched_add_task(task_t *task);
void sched_set_idle_task(task_t *task);
void sched_ready_enqueue(task_t *task);
void sched_ready_remove(task_t *task);
void sched_mark_deleted(task_t *task);
task_t *sched_pop_deleted(void);
void sched_block_current(list_head_t *waiters);
void sched_timeout_insert(task_t *task);
void sched_timeout_remove(task_t *task);
task_t *sched_wake_one(list_head_t *waiters);
task_t *sched_pick_next(void);
int sched_on_tick(void);
void sched_wake_expired(void);
int sched_current_is_idle(void);
void sched_dump_all_task(void);
/*
 * Disable preemptive scheduling only. Interrupts remain enabled, so data shared
 * with interrupt/trap context still needs critical_enter().
 *
 * Blocking operations such as task_sleep() and trt_sem_wait() must not suspend
 * the current task while the scheduler is locked.
 */
void sched_lock(void);
void sched_unlock(void);
int sched_is_locked(void);
void sched_set_pending(void);
void sched_request_from_isr(void);
int sched_try_reschedule_from_trap(void);
