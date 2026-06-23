#pragma once

#include <stdint.h>
#include "error.h"
#include "handle.h"
#include "timer.h"

trt_handle_t trt_sem_create(int max, int cnt);
err_t trt_sem_destroy(trt_handle_t sem);
err_t trt_sem_post(trt_handle_t sem);
err_t trt_sem_post_from_isr(trt_handle_t sem);
err_t trt_sem_wait(trt_handle_t sem);
err_t trt_sem_wait_timeout(trt_handle_t sem, trt_time_t timeout);
err_t trt_sem_trywait(trt_handle_t sem);
