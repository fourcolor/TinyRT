#pragma once

#include <stdint.h>
#include "error.h"
#include "handle.h"
#include "timer.h"

trt_handle_t trt_mutex_create(void);
err_t trt_mutex_destroy(trt_handle_t mutex);
err_t trt_mutex_lock(trt_handle_t mutex);
err_t trt_mutex_lock_timeout(trt_handle_t mutex, trt_time_t timeout);
err_t trt_mutex_trylock(trt_handle_t mutex);
err_t trt_mutex_unlock(trt_handle_t mutex);
