#pragma once

#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "handle.h"
#include "rtos_config.h"
#include "timer.h"

trt_handle_t task_create(const char *name, void (*entry)(void *), void *arg, size_t stack_size,
                         uint8_t priority);
err_t task_delete(trt_handle_t task);
void task_exit(void) __attribute__((noreturn));
void task_yield(void) __attribute__((noinline));
void task_sleep(trt_time_t delay);
void rtos_start(void) __attribute__((noreturn));
