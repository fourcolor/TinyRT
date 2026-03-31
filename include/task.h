#pragma once

#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "list.h"
#include "rtos_config.h"

typedef enum
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DELETED,
} task_state_t;

typedef enum
{
    TASK_WAIT_NONE = 0,
    TASK_WAIT_OBJECT,
    TASK_WAIT_TIMEOUT,
} task_wait_result_t;

typedef struct task_t
{
    char name[20];
    uint32_t *sp;
    uint32_t *stack_base;
    uint32_t stack_size;
    uint32_t wakeup;
    uint8_t waiting_on_timer;
    task_wait_result_t wait_result;
    task_state_t state;
    uint32_t priority;
    uint32_t base_priority;
    uint32_t effective_priority;
    void (*entry)(void *);
    void *arg;
    list_node_t master_list;
    list_node_t state_list;
    list_node_t timeout_list;
} task_t;

void task_init(void);
task_t *task_create(const char *name, void (*entry)(void *), void *arg, size_t stack_size,
                    uint8_t priority);
err_t task_delete(task_t *task);
void task_exit(void) __attribute__((noreturn));
void task_cleanup_deleted(void);
void task_yield(void) __attribute__((noinline));
void task_delay(uint32_t ticks);
void rtos_start(void) __attribute__((noreturn));

void task_start_first(uint32_t *sp) __attribute__((noreturn));
