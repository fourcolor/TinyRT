#pragma once

#define RTOS_TASK_MAX 20
#define RTOS_TASK_STACK_SIZE 2048
#define RTOS_PRIORITY_MIN 0
#define RTOS_PRIORITY_MAX 31
#define RTOS_CPU_CORES BOARD_CPU_CORES

/* Scheduler policy.
 *
 * RTOS_SCHED_PREEMPTIVE:
 *   0 = only switch when the running task blocks or calls task_yield().
 *   1 = systick can preempt the running task.
 *
 * RTOS_SCHED_ROUND_ROBIN:
 *   0 = a running task keeps the CPU until it blocks/yields or a higher
 *       priority task becomes ready.
 *   1 = equal-priority ready tasks share CPU time.
 *
 * RTOS_TIME_SLICE_TICKS:
 *   Number of systicks per round-robin time slice.
 */
#define RTOS_SCHED_PREEMPTIVE 1
#define RTOS_SCHED_ROUND_ROBIN 1
#define RTOS_TIME_SLICE_TICKS 10

#if RTOS_TIME_SLICE_TICKS < 1
#error "RTOS_TIME_SLICE_TICKS must be at least 1"
#endif
