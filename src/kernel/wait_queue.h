#pragma once

#include <stdint.h>
#include "error.h"
#include "list.h"
#include "task_private.h"
#include "timer.h"

typedef struct
{
    list_head_t waiters;
} trt_wait_q_t;

void trt_wait_q_init(trt_wait_q_t *queue);
int trt_wait_q_empty(trt_wait_q_t *queue);
err_t trt_wait_q_block_locked(trt_wait_q_t *queue);
err_t trt_wait_q_block(trt_wait_q_t *queue);
err_t trt_wait_q_block_timeout_locked(trt_wait_q_t *queue, trt_time_t timeout);
err_t trt_wait_q_block_timeout(trt_wait_q_t *queue, trt_time_t timeout);
task_t *trt_wait_q_wake_one_locked(trt_wait_q_t *queue);
task_t *trt_wait_q_wake_one(trt_wait_q_t *queue);
task_t *trt_wait_q_wake_one_from_isr(trt_wait_q_t *queue);
task_t *trt_wait_q_wake_one_result_locked(trt_wait_q_t *queue, task_wait_result_t result);
int trt_wait_q_wake_all_locked(trt_wait_q_t *queue);
int trt_wait_q_wake_all(trt_wait_q_t *queue);
int trt_wait_q_wake_all_from_isr(trt_wait_q_t *queue);
int trt_wait_q_wake_all_result_locked(trt_wait_q_t *queue, task_wait_result_t result);
