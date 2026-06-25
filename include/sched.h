#pragma once

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
