#pragma once

#include <stdint.h>
#include "error.h"
#include "task.h"
#include "timer.h"
#include "wait_queue.h"

typedef struct
{
    task_t *owner;
    uint32_t lock_count;
    trt_wait_q_t waiters;
} trt_mutex_t;

void trt_mutex_init(trt_mutex_t *mutex);
err_t trt_mutex_lock(trt_mutex_t *mutex);
err_t trt_mutex_lock_timeout(trt_mutex_t *mutex, trt_time_t timeout);
err_t trt_mutex_trylock(trt_mutex_t *mutex);
err_t trt_mutex_unlock(trt_mutex_t *mutex);
