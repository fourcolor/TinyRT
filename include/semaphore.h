#pragma once

#include <stdint.h>
#include "error.h"
#include "timer.h"
#include "wait_queue.h"

typedef struct
{
    int cnt;
    int max;
    trt_wait_q_t waiters;
} trt_sem_t;

void trt_sem_init(trt_sem_t *sem, int max, int cnt);
trt_sem_t *trt_sem_create(int max, int cnt);
err_t trt_sem_post(trt_sem_t *sem);
err_t trt_sem_post_from_isr(trt_sem_t *sem);
err_t trt_sem_wait(trt_sem_t *sem);
err_t trt_sem_wait_timeout(trt_sem_t *sem, trt_time_t timeout);
err_t trt_sem_trywait(trt_sem_t *sem);
