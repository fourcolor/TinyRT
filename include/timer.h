#pragma once

#include <stdint.h>
#include "error.h"
#include "handle.h"

typedef void (*timer_callback_t)(void *);

typedef struct
{
    uint64_t us;
} trt_time_t;

#define TRT_US(value) ((trt_time_t){.us = (uint64_t)(value)})
#define TRT_MS(value) ((trt_time_t){.us = (uint64_t)(value)*1000ull})
#define TRT_SEC(value) ((trt_time_t){.us = (uint64_t)(value)*1000000ull})
#define TRT_TIME_FOREVER_US UINT64_MAX
#define TRT_WAIT_FOREVER TRT_US(TRT_TIME_FOREVER_US)

uint32_t timer_ticks(void);
uint64_t timer_cycles(void);
uint64_t timer_us(void);
uint32_t timer_us_to_ticks(uint64_t us);
uint32_t timer_ms_to_ticks(uint64_t ms);
uint32_t timer_sec_to_ticks(uint64_t sec);
int timer_expired(uint32_t now, uint32_t deadline);

trt_handle_t trt_timer_create(timer_callback_t callback, void *arg);
void trt_timer_start(trt_handle_t timer, trt_time_t delay, trt_time_t period);
void trt_timer_stop(trt_handle_t timer);
err_t trt_timer_destroy(trt_handle_t timer);
int trt_timer_active(trt_handle_t timer);
